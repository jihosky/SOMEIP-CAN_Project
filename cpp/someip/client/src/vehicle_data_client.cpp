#include "vehicle_someip/identifiers.hpp"
#include "vehicle_someip/payload_codec.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <vsomeip/vsomeip.hpp>

namespace {

class VehicleDataClient {
public:
    VehicleDataClient(std::chrono::milliseconds timeout, bool subscribe,
                      std::size_t event_count)
        : timeout_(timeout), subscribe_(subscribe), event_count_(event_count),
          application_(vsomeip::runtime::get()->create_application("vehicle-client")) {}

    int run() {
        if (!application_ || !application_->init()) {
            std::cerr << "Could not initialize vSomeIP client; check VSOMEIP_CONFIGURATION\n";
            return 1;
        }
        application_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state != vsomeip::state_type_e::ST_REGISTERED) return;
            application_->request_service(vehicle_someip::service_id,
                                          vehicle_someip::instance_id);
            if (subscribe_) {
                application_->request_event(
                    vehicle_someip::service_id, vehicle_someip::instance_id,
                    vehicle_someip::vehicle_data_event_id,
                    {vehicle_someip::vehicle_data_eventgroup_id},
                    vsomeip::event_type_e::ET_EVENT,
                    vsomeip::reliability_type_e::RT_RELIABLE);
            }
        });
        application_->register_availability_handler(
            vehicle_someip::service_id, vehicle_someip::instance_id,
            [this](vsomeip::service_t, vsomeip::instance_t, bool available) {
                on_availability(available);
            });
        if (subscribe_) {
            application_->register_message_handler(
                vehicle_someip::service_id, vehicle_someip::instance_id,
                vehicle_someip::vehicle_data_event_id,
                [this](const std::shared_ptr<vsomeip::message>& message) {
                    on_event(message);
                });
        } else {
            for (const auto method : methods_) {
                application_->register_message_handler(
                    vehicle_someip::service_id, vehicle_someip::instance_id, method,
                    [this](const std::shared_ptr<vsomeip::message>& response) {
                        on_response(response);
                    });
            }
        }

        std::thread deadline([this] {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!condition_.wait_for(lock, timeout_, [this] { return completed_; })) {
                error_ = available_ ? (subscribe_
                    ? "Timed out waiting for VehicleData events"
                    : "Timed out waiting for SOME/IP responses")
                    : "VehicleDataService unavailable before timeout";
            }
            lock.unlock();
            application_->stop();
        });
        application_->start();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            completed_ = true;
        }
        condition_.notify_all();
        deadline.join();
        if (!error_.empty()) {
            std::cerr << error_ << '\n';
            return 1;
        }
        return 0;
    }

private:
    void on_availability(bool available) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            available_ = available;
            if (!available || requested_) return;
            requested_ = true;
        }
        if (subscribe_) {
            application_->subscribe(
                vehicle_someip::service_id, vehicle_someip::instance_id,
                vehicle_someip::vehicle_data_eventgroup_id, vsomeip::DEFAULT_MAJOR,
                vehicle_someip::vehicle_data_event_id);
            std::cout << "Subscribed to VehicleData events" << std::endl;
            return;
        }
        for (const auto method : methods_) {
            auto request = vsomeip::runtime::get()->create_request();
            request->set_service(vehicle_someip::service_id);
            request->set_instance(vehicle_someip::instance_id);
            request->set_method(method);
            application_->send(request);
        }
    }

    void on_event(const std::shared_ptr<vsomeip::message>& message) {
        const auto payload = message->get_payload();
        const auto data = payload
            ? vehicle_someip::decode_vehicle_data(payload->get_data(),
                                                  payload->get_length())
            : std::nullopt;
        std::lock_guard<std::mutex> lock(mutex_);
        if (completed_) return;
        if (!data) {
            error_ = "Malformed VehicleData event payload";
            completed_ = true;
            condition_.notify_all();
            return;
        }
        std::cout << "VehicleData event: speed=" << std::fixed
                  << std::setprecision(2) << data->vehicle_speed_kph
                  << " km/h rpm=" << data->engine_rpm
                  << " coolant=" << data->coolant_temperature_c << " C"
                  << std::endl;
        if (++events_received_ >= event_count_) {
            completed_ = true;
            condition_.notify_all();
        }
    }

    void on_response(const std::shared_ptr<vsomeip::message>& response) {
        std::string line;
        if (response->get_return_code() != vsomeip::return_code_e::E_OK) {
            line = "Vehicle data unavailable (SOME/IP return code " +
                   std::to_string(static_cast<int>(response->get_return_code())) + ")";
        } else {
            const auto payload = response->get_payload();
            if (!payload) {
                line = "Malformed empty SOME/IP response";
            } else {
                const auto* data = payload->get_data();
                const auto length = payload->get_length();
                std::ostringstream stream;
                switch (response->get_method()) {
                case vehicle_someip::get_vehicle_speed_id:
                    if (const auto value = vehicle_someip::decode_speed(data, length))
                        stream << "Vehicle speed: " << std::fixed << std::setprecision(2)
                               << *value << " km/h";
                    break;
                case vehicle_someip::get_engine_rpm_id:
                    if (const auto value = vehicle_someip::decode_rpm(data, length))
                        stream << "Engine RPM: " << *value << " rpm";
                    break;
                case vehicle_someip::get_coolant_temperature_id:
                    if (const auto value = vehicle_someip::decode_temperature(data, length))
                        stream << "Coolant temperature: " << *value << " C";
                    break;
                }
                line = stream.str().empty() ? "Malformed SOME/IP payload" : stream.str();
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (completed_) return;
        for (std::size_t index = 0; index < methods_.size(); ++index) {
            if (response->get_method() == methods_[index] && !received_[index]) {
                received_[index] = true;
                std::cout << line << std::endl;
                if (line.find("Malformed") == 0 ||
                    line.find("unavailable") != std::string::npos)
                    error_ = line;
                break;
            }
        }
        if (received_[0] && received_[1] && received_[2]) {
            completed_ = true;
            condition_.notify_all();
        }
    }

    const std::chrono::milliseconds timeout_;
    const bool subscribe_;
    const std::size_t event_count_;
    std::shared_ptr<vsomeip::application> application_;
    std::mutex mutex_;
    std::condition_variable condition_;
    const std::array<std::uint16_t, 3> methods_{
        vehicle_someip::get_vehicle_speed_id, vehicle_someip::get_engine_rpm_id,
        vehicle_someip::get_coolant_temperature_id};
    std::array<bool, 3> received_{};
    std::size_t events_received_ = 0;
    bool available_ = false;
    bool requested_ = false;
    bool completed_ = false;
    std::string error_;
};

}  // namespace

int main(int argc, char* argv[]) {
    int timeout_seconds = 5;
    std::size_t event_count = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        try {
            if (argument == "--timeout" && index + 1 < argc) {
                timeout_seconds = std::stoi(argv[++index]);
            } else if (argument == "--subscribe" && index + 1 < argc) {
                event_count = static_cast<std::size_t>(std::stoul(argv[++index]));
            } else {
                throw std::invalid_argument("argument");
            }
        } catch (...) {
            std::cerr << "Usage: " << argv[0]
                      << " [--timeout SECONDS] [--subscribe EVENT_COUNT]\n";
            return 2;
        }
    }
    if (timeout_seconds <= 0 || timeout_seconds > 300 || event_count > 10000) {
        std::cerr << "Timeout must be 1-300 seconds and event count 1-10000\n";
        return 2;
    }
    VehicleDataClient client{std::chrono::seconds(timeout_seconds),
                             event_count != 0, event_count};
    return client.run();
}
