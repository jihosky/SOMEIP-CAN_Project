#include "vehicle_someip/identifiers.hpp"
#include "vehicle_someip/payload_codec.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <unistd.h>
#include <vsomeip/vsomeip.hpp>

namespace {

class DashboardClient {
public:
    DashboardClient()
        : application_(vsomeip::runtime::get()->create_application("vehicle-client")) {}

    int run() {
        if (!application_ || !application_->init()) {
            std::cerr << "Could not initialize vSomeIP dashboard client\n";
            return 1;
        }
        application_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state != vsomeip::state_type_e::ST_REGISTERED) return;
            application_->request_service(vehicle_someip::service_id,
                                          vehicle_someip::instance_id);
            for (const auto event : {
                     std::pair{vehicle_someip::vehicle_data_event_id,
                               vehicle_someip::vehicle_data_eventgroup_id},
                     std::pair{vehicle_someip::body_status_event_id,
                               vehicle_someip::body_status_eventgroup_id}}) {
                application_->request_event(
                    vehicle_someip::service_id, vehicle_someip::instance_id,
                    event.first, {event.second}, vsomeip::event_type_e::ET_EVENT,
                    vsomeip::reliability_type_e::RT_RELIABLE);
            }
        });
        application_->register_availability_handler(
            vehicle_someip::service_id, vehicle_someip::instance_id,
            [this](vsomeip::service_t, vsomeip::instance_t, bool available) {
                on_availability(available);
            });
        application_->register_message_handler(
            vehicle_someip::service_id, vehicle_someip::instance_id,
            vsomeip::ANY_METHOD,
            [this](const std::shared_ptr<vsomeip::message>& message) {
                on_message(message);
            });
        std::thread input([this] {
            std::string line;
            pollfd descriptor{STDIN_FILENO, POLLIN, 0};
            while (running_) {
                const int ready = poll(&descriptor, 1, 100);
                if (ready < 0 || ready == 0) continue;
                if (descriptor.revents & (POLLHUP | POLLERR)) {
                    application_->stop();
                    return;
                }
                if (!(descriptor.revents & POLLIN)) continue;
                if (!std::getline(std::cin, line)) {
                    application_->stop();
                    return;
                }
                std::istringstream stream(line);
                std::string operation, action, extra;
                int door = -1;
                if (!(stream >> operation >> door >> action) || (stream >> extra) ||
                    operation != "door" || door != 0 ||
                    (action != "open" && action != "close")) {
                    emit("DASHBOARD ERROR invalid-command");
                    continue;
                }
                send_door(static_cast<std::uint8_t>(door), action == "open");
            }
        });
        application_->start();
        running_ = false;
        input.join();
        return 0;
    }

private:
    void emit(const std::string& line) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        std::cout << line << std::endl;
    }

    void on_availability(bool available) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            available_ = available;
            if (!available) pending_ = false;
        }
        emit(available ? "DASHBOARD SERVICE available" :
                         "DASHBOARD SERVICE unavailable");
        if (!available) return;
        for (const auto group : {vehicle_someip::vehicle_data_eventgroup_id,
                                 vehicle_someip::body_status_eventgroup_id}) {
            application_->subscribe(vehicle_someip::service_id,
                                    vehicle_someip::instance_id, group);
        }
        auto request = vsomeip::runtime::get()->create_request();
        request->set_service(vehicle_someip::service_id);
        request->set_instance(vehicle_someip::instance_id);
        request->set_method(vehicle_someip::get_body_status_id);
        application_->send(request);
    }

    void send_door(std::uint8_t door, bool open) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!available_) {
                emit("DASHBOARD ERROR service-unavailable");
                return;
            }
            if (pending_ && std::chrono::steady_clock::now() - pending_since_ <
                                std::chrono::seconds(6)) {
                emit("DASHBOARD ERROR command-busy");
                return;
            }
            pending_ = true;
            pending_door_ = door;
            pending_open_ = open;
            pending_since_ = std::chrono::steady_clock::now();
        }
        auto request = vsomeip::runtime::get()->create_request();
        request->set_service(vehicle_someip::service_id);
        request->set_instance(vehicle_someip::instance_id);
        request->set_method(vehicle_someip::set_door_id);
        request->set_payload(vsomeip::runtime::get()->create_payload(
            std::vector<std::uint8_t>{door, static_cast<std::uint8_t>(open)}));
        application_->send(request);
    }

    void on_message(const std::shared_ptr<vsomeip::message>& message) {
        const auto payload = message->get_payload();
        const auto* bytes = payload ? payload->get_data() : nullptr;
        const auto length = payload ? payload->get_length() : 0;
        std::ostringstream line;
        if (message->get_method() == vehicle_someip::vehicle_data_event_id) {
            const auto value = vehicle_someip::decode_vehicle_data(bytes, length);
            if (!value) return;
            line << "DASHBOARD DATA speed=" << std::fixed << std::setprecision(2)
                 << value->vehicle_speed_kph << " rpm=" << value->engine_rpm
                 << " coolant=" << value->coolant_temperature_c;
        } else if (message->get_method() == vehicle_someip::body_status_event_id ||
                   message->get_method() == vehicle_someip::get_body_status_id) {
            const auto value = vehicle_someip::decode_body_status(bytes, length);
            if (!value) return;
            line << (message->get_method() == vehicle_someip::body_status_event_id
                         ? "DASHBOARD BODY_EVENT flags="
                         : "DASHBOARD BODY_RESPONSE flags=")
                 << static_cast<unsigned int>(value->flags);
        } else if (message->get_method() == vehicle_someip::set_door_id) {
            std::uint8_t door = 0;
            bool open = false;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (!pending_) return;
                door = pending_door_;
                open = pending_open_;
                pending_ = false;
            }
            if (message->get_return_code() != vsomeip::return_code_e::E_OK ||
                length != 1 || bytes == nullptr || bytes[0] != 1) {
                line << "DASHBOARD ERROR command-rejected";
            } else {
                line << "DASHBOARD ACK door=" << static_cast<unsigned int>(door)
                     << " action=" << (open ? "open" : "close");
            }
        } else {
            return;
        }
        emit(line.str());
    }

    std::shared_ptr<vsomeip::application> application_;
    std::atomic<bool> running_{true};
    std::mutex state_mutex_;
    std::mutex output_mutex_;
    bool available_ = false;
    bool pending_ = false;
    std::uint8_t pending_door_ = 0;
    bool pending_open_ = false;
    std::chrono::steady_clock::time_point pending_since_{};
};

}  // namespace

int main() {
    return DashboardClient{}.run();
}
