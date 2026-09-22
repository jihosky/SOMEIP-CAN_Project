#pragma once

#include <chrono>
#include <cstdint>

namespace vehicle_service {

struct VehicleData {
    double vehicle_speed_kph{};
    std::uint16_t engine_rpm{};
    std::int16_t coolant_temperature_c{};
    std::chrono::system_clock::time_point timestamp{};
};

}  // namespace vehicle_service
