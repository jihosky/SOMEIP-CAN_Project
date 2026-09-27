// Test-only sample injection: exercises the production provider without vCAN.
#include "vehicle_someip/vehicle_data_provider.hpp"
#include "vehicle_someip/identifiers.hpp"
#include <iostream>
#include <sstream>
#include <thread>

int main() {
    vehicle_service::VehicleService service;
    auto app = vsomeip::runtime::get()->create_application("vehicle-provider");
    vehicle_someip::VehicleDataProvider provider(app, service);
    if (!provider.init()) return 1;
    std::thread runtime([&] { provider.start(); });
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        if (line == "malformed") {
            app->notify(vehicle_someip::service_id, vehicle_someip::instance_id,
                        vehicle_someip::vehicle_data_event_id,
                        vsomeip::runtime::get()->create_payload(std::vector<std::uint8_t>{1}), true);
            continue;
        }
        vehicle_service::VehicleData sample{};
        std::istringstream input(line);
        if (input >> sample.vehicle_speed_kph >> sample.engine_rpm >> sample.coolant_temperature_c) {
            service.update(sample);
            provider.publish(sample);
        }
    }
    app->stop();
    runtime.join();
}
