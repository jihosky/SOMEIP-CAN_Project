#include "vehicle_someip/identifiers.hpp"
#include "vehicle_someip/payload_codec.hpp"

#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <vsomeip/vsomeip.hpp>

namespace {

enum class Mode { read, subscribe, command };

void print_status(std::uint8_t flags) {
    const char* names[] = {"driver_door", "passenger_door", "rear_left_door",
                           "rear_right_door", "trunk"};
    std::cout << "BodyStatus flags=0x" << std::hex
              << static_cast<unsigned int>(flags) << std::dec;
    for (unsigned int bit = 0; bit < 5; ++bit) {
        std::cout << ' ' << names[bit] << '='
                  << ((flags & (1u << bit)) ? "open" : "closed");
    }
    std::cout << " doors_locked=" << ((flags & 0x20) ? "yes" : "no")
              << " headlights=" << ((flags & 0x40) ? "on" : "off")
              << std::endl;
}

class BodyClient {
public:
    BodyClient(Mode mode, unsigned int count, std::uint8_t door, bool open)
        : mode_(mode), count_(count), door_(door), open_(open),
          app_(vsomeip::runtime::get()->create_application("vehicle-client")) {}

    int run() {
        if (!app_ || !app_->init()) {
            std::cerr << "Could not initialize vSomeIP body client\n";
            return 1;
        }
        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state != vsomeip::state_type_e::ST_REGISTERED) return;
            app_->request_service(vehicle_someip::service_id, vehicle_someip::instance_id);
            if (mode_ != Mode::read) {
                app_->request_event(vehicle_someip::service_id, vehicle_someip::instance_id,
                                    vehicle_someip::body_status_event_id,
                                    {vehicle_someip::body_status_eventgroup_id},
                                    vsomeip::event_type_e::ET_EVENT,
                                    vsomeip::reliability_type_e::RT_RELIABLE);
            }
        });
        app_->register_availability_handler(
            vehicle_someip::service_id, vehicle_someip::instance_id,
            [this](vsomeip::service_t, vsomeip::instance_t, bool available) {
                if (!available || requested_.exchange(true)) return;
                if (mode_ != Mode::read) {
                    app_->subscribe(vehicle_someip::service_id, vehicle_someip::instance_id,
                                    vehicle_someip::body_status_eventgroup_id,
                                    vsomeip::DEFAULT_MAJOR,
                                    vehicle_someip::body_status_event_id);
                    if (mode_ == Mode::subscribe) {
                        std::cout << "Subscribed to BodyStatus events" << std::endl;
                        return;
                    }
                }
                auto request = vsomeip::runtime::get()->create_request();
                request->set_service(vehicle_someip::service_id);
                request->set_instance(vehicle_someip::instance_id);
                request->set_method(mode_ == Mode::read
                    ? vehicle_someip::get_body_status_id : vehicle_someip::set_door_id);
                if (mode_ == Mode::command) {
                    request->set_payload(vsomeip::runtime::get()->create_payload(
                        std::vector<std::uint8_t>{door_, static_cast<std::uint8_t>(open_)}));
                }
                app_->send(request);
            });
        app_->register_message_handler(
            vehicle_someip::service_id, vehicle_someip::instance_id,
            vehicle_someip::body_status_event_id,
            [this](const std::shared_ptr<vsomeip::message>& message) {
                const auto payload = message->get_payload();
                const auto status = payload ? vehicle_someip::decode_body_status(
                    payload->get_data(), payload->get_length()) : std::nullopt;
                std::lock_guard<std::mutex> lock(mutex_);
                if (!status) {
                    error_ = "Malformed BodyStatus event";
                    done_ = true;
                } else {
                    print_status(status->flags);
                    if (mode_ == Mode::subscribe && ++received_ >= count_) done_ = true;
                    if (mode_ == Mode::command && accepted_ &&
                        ((status->flags >> door_) & 1) == static_cast<unsigned int>(open_)) {
                        done_ = true;
                    }
                }
                condition_.notify_all();
            });
        for (const auto method : {vehicle_someip::get_body_status_id,
                                  vehicle_someip::set_door_id}) {
            app_->register_message_handler(
                vehicle_someip::service_id, vehicle_someip::instance_id, method,
                [this](const std::shared_ptr<vsomeip::message>& message) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (message->get_return_code() != vsomeip::return_code_e::E_OK) {
                        error_ = "Body request failed with SOME/IP return code " +
                                 std::to_string(static_cast<int>(message->get_return_code()));
                        done_ = true;
                    } else if (message->get_method() == vehicle_someip::get_body_status_id) {
                        const auto payload = message->get_payload();
                        const auto status = payload ? vehicle_someip::decode_body_status(
                            payload->get_data(), payload->get_length()) : std::nullopt;
                        if (!status) error_ = "Malformed BodyStatus response";
                        else print_status(status->flags);
                        done_ = true;
                    } else {
                        accepted_ = true;
                        std::cout << "Door command accepted; waiting for CAN status"
                                  << std::endl;
                    }
                    condition_.notify_all();
                });
        }
        std::thread timer([this] {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!condition_.wait_for(lock, std::chrono::seconds(10), [this] { return done_; })) {
                error_ = "Timed out waiting for BodyStatus";
            }
            lock.unlock();
            app_->stop();
        });
        app_->start();
        timer.join();
        if (!error_.empty()) {
            std::cerr << error_ << '\n';
            return 1;
        }
        return 0;
    }

private:
    Mode mode_;
    unsigned int count_;
    std::uint8_t door_;
    bool open_;
    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> requested_{false};
    bool accepted_ = false;
    bool done_ = false;
    unsigned int received_ = 0;
    std::string error_;
};

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "read")
        return BodyClient(Mode::read, 0, 0, false).run();
    if (argc == 3 && std::string(argv[1]) == "subscribe") {
        try {
            const auto count = std::stoul(argv[2]);
            if (count >= 1 && count <= 10000)
                return BodyClient(Mode::subscribe, static_cast<unsigned int>(count), 0, false).run();
        } catch (...) {}
    }
    if (argc == 4 && std::string(argv[1]) == "door") {
        try {
            const auto door = std::stoul(argv[2]);
            const std::string action = argv[3];
            if (door <= 4 && (action == "open" || action == "close"))
                return BodyClient(Mode::command, 0, static_cast<std::uint8_t>(door),
                                  action == "open").run();
        } catch (...) {}
    }
    std::cerr << "Usage: " << argv[0]
              << " read | subscribe COUNT | door INDEX(0-4) open|close\n";
    return 2;
}
