#include "can_gateway/frame_format.hpp"
#include "can_gateway/signal_decoder.hpp"
#include "can_gateway/socketcan_receiver.hpp"
#include "vehicle_service/vehicle_data_format.hpp"
#include "vehicle_service/vehicle_service.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
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
    std::string error;
    if (!receiver.open(interface_name, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::atomic<bool> finished{false};
    std::thread stopper([&] {
        while (!stop_signal && !finished.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        receiver.stop();
    });

    const can_gateway::SignalDecoder decoder;
    vehicle_service::VehicleService service;
    const bool okay = receiver.run([&](const can_gateway::CanFrame& frame) {
        std::cout << can_gateway::format_frame(frame) << std::endl;
        if (const auto data = decoder.decode(frame)) {
            service.update(*data);
            std::cout << vehicle_service::format_vehicle_data(*data) << std::endl;
        }
    }, error);
    finished.store(true);
    stopper.join();
    if (!okay) {
        std::cerr << error << '\n';
        return 1;
    }
    return 0;
}
