#include "vehicle_someip/payload_codec.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace vehicle_someip {

std::vector<std::uint8_t> encode_speed(double kph) {
    if (!std::isfinite(kph) || kph < 0 ||
        kph > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) / 100.0) {
        throw std::out_of_range("vehicle speed cannot be encoded");
    }
    const auto raw = static_cast<std::uint32_t>(std::llround(kph * 100.0));
    return {static_cast<std::uint8_t>(raw >> 24), static_cast<std::uint8_t>(raw >> 16),
            static_cast<std::uint8_t>(raw >> 8), static_cast<std::uint8_t>(raw)};
}

std::vector<std::uint8_t> encode_rpm(std::uint16_t rpm) {
    return {static_cast<std::uint8_t>(rpm >> 8), static_cast<std::uint8_t>(rpm)};
}

std::vector<std::uint8_t> encode_temperature(std::int16_t celsius) {
    const auto raw = static_cast<std::uint16_t>(celsius);
    return {static_cast<std::uint8_t>(raw >> 8), static_cast<std::uint8_t>(raw)};
}

std::optional<double> decode_speed(const std::uint8_t* data, std::size_t length) {
    if (length != 4 || data == nullptr) return std::nullopt;
    const auto raw = (static_cast<std::uint32_t>(data[0]) << 24) |
                     (static_cast<std::uint32_t>(data[1]) << 16) |
                     (static_cast<std::uint32_t>(data[2]) << 8) | data[3];
    return static_cast<double>(raw) / 100.0;
}

std::optional<std::uint16_t> decode_rpm(const std::uint8_t* data, std::size_t length) {
    if (length != 2 || data == nullptr) return std::nullopt;
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) | data[1]);
}

std::optional<std::int16_t> decode_temperature(const std::uint8_t* data,
                                                std::size_t length) {
    const auto raw = decode_rpm(data, length);
    if (!raw) return std::nullopt;
    const auto signed_value = *raw < 0x8000 ? static_cast<std::int32_t>(*raw)
                                            : static_cast<std::int32_t>(*raw) - 0x10000;
    return static_cast<std::int16_t>(signed_value);
}

}  // namespace vehicle_someip
