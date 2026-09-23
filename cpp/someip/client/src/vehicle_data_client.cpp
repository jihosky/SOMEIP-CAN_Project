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
    explicit VehicleDataClient(std::chrono::milliseconds timeout)
        : timeout_(timeout), application_(vsomeip::runtime::get()->create_application("vehicle-client")) {}

    int run() {
        if (!application_ || !application_->init()) {
            std::cerr << "Could not initialize vSomeIP client; check VSOMEIP_CONFIGURATION\n";
            return 1;
        }
        application_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                application_->request_service(vehicle_someip::service_id,
                                              vehicle_someip::instance_id);
            }
        });
        application_->register_availability_handler(
            vehicle_someip::service_id, vehicle_someip::instance_id,
            [this](vsomeip::service_t, vsomeip::instance_t, bool available) {
                on_availability(available);
            });
        for (const auto method : methods_) {
            application_->register_message_handler(
                vehicle_someip::service_id, vehicle_someip::instance_id, method,
                [this](const std::shared_ptr<vsomeip::message>& response) {
                    on_response(response);
                });
        }

        std::thread deadline([this] {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!condition_.wait_for(lock, timeout_, [this] { return completed_; })) {
                error_ = available_ ? "Timed out waiting for SOME/IP responses"
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
        for (const auto method : methods_) {
            auto request = vsomeip::runtime::get()->create_request();
            request->set_service(vehicle_someip::service_id);
            request->set_instance(vehicle_someip::instance_id);
            request->set_method(method);
            application_->send(request);
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
                    if (const auto value = vehicle_someip::decode_speed(data, length)) {
                        stream << "Vehicle speed: " << std::fixed << std::setprecision(2)
                               << *value << " km/h";
                    }
                    break;
                case vehicle_someip::get_engine_rpm_id:
                    if (const auto value = vehicle_someip::decode_rpm(data, length)) {
                        stream << "Engine RPM: " << *value << " rpm";
                    }
                    break;
                case vehicle_someip::get_coolant_temperature_id:
                    if (const auto value = vehicle_someip::decode_temperature(data, length)) {
                        stream << "Coolant temperature: " << *value << " C";
                    }
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
                if (line.find("Malformed") == 0 || line.find("unavailable") != std::string::npos) {
                    error_ = line;
                }
                break;
            }
        }
        if (received_[0] && received_[1] && received_[2]) {
            completed_ = true;
            condition_.notify_all();
        }
    }

    const std::chrono::milliseconds timeout_;
    std::shared_ptr<vsomeip::application> application_;
    std::mutex mutex_;
    std::condition_variable condition_;
    const std::array<std::uint16_t, 3> methods_{
        vehicle_someip::get_vehicle_speed_id, vehicle_someip::get_engine_rpm_id,
        vehicle_someip::get_coolant_temperature_id};
    std::array<bool, 3> received_{};
    bool available_ = false;
    bool requested_ = false;
    bool completed_ = false;
    std::string error_;
};

}  // namespace

int main(int argc, char* argv[]) {
    int timeout_seconds = 5;
    if (argc == 3 && std::string(argv[1]) == "--timeout") {
        try {
            timeout_seconds = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid timeout\n";
            return 2;
        }
        if (timeout_seconds <= 0 || timeout_seconds > 300) {
            std::cerr << "Timeout must be between 1 and 300 seconds\n";
            return 2;
        }
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [--timeout SECONDS]\n";
        return 2;
    }
    VehicleDataClient client{std::chrono::seconds(timeout_seconds)};
    return client.run();
}
