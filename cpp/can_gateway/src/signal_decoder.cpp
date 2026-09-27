#include "can_gateway/signal_decoder.hpp"

#include "milestone1_definition.hpp"
#include "body_definition.hpp"

#include <cstdint>

namespace can_gateway {
namespace {

std::uint32_t read_little_endian(const CanFrame& frame, std::uint8_t start,
                                 std::uint8_t length) {
    std::uint32_t value = 0;
    for (std::uint8_t index = 0; index < length; ++index) {
        value |= static_cast<std::uint32_t>(frame.payload[start + index]) << (8 * index);
    }
    return value;
}

}  // namespace

std::optional<vehicle_service::VehicleData> SignalDecoder::decode(
    const CanFrame& frame) const {
    using namespace milestone1_definition;
    if (frame.id != can_id || frame.dlc != dlc) {
        return std::nullopt;
    }

    const auto raw_speed = read_little_endian(frame, speed_start, speed_length);
    const auto raw_rpm = read_little_endian(frame, rpm_start, rpm_length);
    const auto raw_temperature = read_little_endian(frame, temperature_start,
                                                     temperature_length);
    return vehicle_service::VehicleData{
        static_cast<double>(raw_speed) * speed_scale,
        static_cast<std::uint16_t>(raw_rpm),
        static_cast<std::int16_t>(static_cast<int>(raw_temperature) + temperature_offset),
        frame.received_at,
    };
}

std::optional<vehicle_service::BodyStatus> SignalDecoder::decode_body(
    const CanFrame& frame) const {
    if (frame.id != body_definition::can_id || frame.dlc != body_definition::dlc ||
        (frame.payload[0] & 0x80) != 0) {
        return std::nullopt;
    }
    return vehicle_service::BodyStatus{frame.payload[0], frame.received_at};
}

}  // namespace can_gateway
