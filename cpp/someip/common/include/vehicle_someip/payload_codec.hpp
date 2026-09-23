#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace vehicle_someip {

std::vector<std::uint8_t> encode_speed(double kph);
std::vector<std::uint8_t> encode_rpm(std::uint16_t rpm);
std::vector<std::uint8_t> encode_temperature(std::int16_t celsius);

std::optional<double> decode_speed(const std::uint8_t* data, std::size_t length);
std::optional<std::uint16_t> decode_rpm(const std::uint8_t* data, std::size_t length);
std::optional<std::int16_t> decode_temperature(const std::uint8_t* data, std::size_t length);

}  // namespace vehicle_someip
