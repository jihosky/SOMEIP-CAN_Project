#include "can_gateway/signal_decoder.hpp"
#include "can_gateway/body_command_sender.hpp"
#include "can_gateway/socketcan_receiver.hpp"
#include "vehicle_service/vehicle_data_format.hpp"
#include "vehicle_service/vehicle_service.hpp"
#include "vehicle_someip/vehicle_data_provider.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t stop_signal = 0;

void on_signal(int) { stop_signal = 1; }

}  // namespace

int main(int argc, char* argv[]) {
    std::string interface_name = "vcan0";
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " [--interface NAME]\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--interface") {
        interface_name = argv[2];
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [--interface NAME]\n";
        return 2;
    }

    can_gateway::SocketCanReceiver receiver;
    can_gateway::BodyCommandSender command_sender;
    std::string error;
    if (!receiver.open(interface_name, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    const bool body_commands_enabled = command_sender.open(interface_name, error);
    if (!body_commands_enabled) {
        std::cerr << "Body commands unavailable: " << error << '\n';
    }

    vehicle_service::VehicleService service;
    const can_gateway::SignalDecoder decoder;
    auto application = vsomeip::runtime::get()->create_application("vehicle-provider");
    vehicle_someip::VehicleDataProvider provider(
        application, service,
        body_commands_enabled
            ? std::function<bool(std::uint8_t, bool)>(
                  [&command_sender](std::uint8_t door, bool open) {
                      std::string send_error;
                      if (command_sender.send(door, open, send_error)) return true;
                      std::cerr << send_error << '\n';
                      return false;
                  })
            : std::function<bool(std::uint8_t, bool)>{});
    const char* configuration = std::getenv("VSOMEIP_CONFIGURATION");
    std::cerr << "[SOMEIP] application=" << application->get_name()
              << " VSOMEIP_CONFIGURATION="
              << (configuration ? configuration : "(unset)") << std::endl;
    if (!provider.init()) {
        std::cerr << "Could not initialize vSomeIP provider; check VSOMEIP_CONFIGURATION\n";
        return 1;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::atomic<bool> can_done{false};
    std::atomic<bool> can_okay{true};
    std::atomic<bool> app_done{false};
    std::thread can_thread([&] {
        std::string receive_error;
        const bool okay = receiver.run([&](const can_gateway::CanFrame& frame) {
            if (const auto data = decoder.decode(frame)) {
                service.update(*data);
                provider.publish(*data);
                std::cout << vehicle_service::format_vehicle_data(*data) << std::endl;
            } else if (const auto body = decoder.decode_body(frame)) {
                service.updateBody(*body);
                provider.publishBody(*body);
                std::cout << "body_status_flags=0x" << std::hex
                          << static_cast<unsigned int>(body->flags) << std::dec
                          << std::endl;
            }
        }, receive_error);
        if (!okay) {
            std::cerr << receive_error << '\n';
            can_okay.store(false);
        }
        can_done.store(true);
    });
    std::thread stopper([&] {
        while (!stop_signal && !can_done.load() && !app_done.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        receiver.stop();
        application->stop();
    });

    std::cout << "Integrated vehicle gateway ready on " << interface_name
              << " (vSomeIP registration pending)" << std::endl;
    provider.start();
    app_done.store(true);
    receiver.stop();
    stopper.join();
    can_thread.join();
    return can_okay.load() ? 0 : 1;
}
