#include "vehicle_service/vehicle_data_format.hpp"
#include "vehicle_service/vehicle_service.hpp"

#include <chrono>
#include <iostream>
#include <string>

int main() {
    vehicle_service::VehicleService service;
    if (service.latest() || service.getVehicleSpeed() || service.getEngineRpm() ||
        service.getCoolantTemperature()) {
        std::cerr << "Empty service should have no values\n";
        return 1;
    }

    const vehicle_service::VehicleData data{
        123.45, 2500, 85, std::chrono::system_clock::time_point{}};
    service.update(data);
    if (service.getVehicleSpeed() != 123.45 || service.getEngineRpm() != 2500 ||
        service.getCoolantTemperature() != 85 ||
        service.latest()->timestamp != data.timestamp) {
        std::cerr << "Service did not expose latest vehicle data\n";
        return 1;
    }

    const std::string expected =
        "1970-01-01T00:00:00.000Z vehicle_speed_kph=123.45 "
        "engine_rpm=2500 coolant_temperature_c=85";
    if (vehicle_service::format_vehicle_data(data) != expected) {
        std::cerr << "Unexpected vehicle data output\n";
        return 1;
    }
    return 0;
}
