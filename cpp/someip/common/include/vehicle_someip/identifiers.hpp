#pragma once

#include <cstdint>

namespace vehicle_someip {

constexpr std::uint16_t service_id = 0x6301;
constexpr std::uint16_t instance_id = 0x0001;
constexpr std::uint16_t get_vehicle_speed_id = 0x0001;
constexpr std::uint16_t get_engine_rpm_id = 0x0002;
constexpr std::uint16_t get_coolant_temperature_id = 0x0003;

}  // namespace vehicle_someip
