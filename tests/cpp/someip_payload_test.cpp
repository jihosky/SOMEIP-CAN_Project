#include "vehicle_someip/payload_codec.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

int main() {
    using namespace vehicle_someip;
    const std::vector<std::uint8_t> speed{0x00, 0x00, 0x30, 0x39};
    const std::vector<std::uint8_t> rpm{0x09, 0xC4};
    const std::vector<std::uint8_t> temperature{0xFF, 0xD8};
    if (encode_speed(123.45) != speed || encode_rpm(2500) != rpm ||
        encode_temperature(-40) != temperature) {
        std::cerr << "Incorrect big-endian payload bytes\n";
        return 1;
    }
    if (decode_speed(speed.data(), speed.size()) != 123.45 ||
        decode_rpm(rpm.data(), rpm.size()) != 2500 ||
        decode_temperature(temperature.data(), temperature.size()) != -40) {
        std::cerr << "Incorrect payload decoding\n";
        return 1;
    }
    if (decode_speed(speed.data(), 3) || decode_rpm(nullptr, 2) ||
        decode_temperature(temperature.data(), 1)) {
        std::cerr << "Malformed payload was accepted\n";
        return 1;
    }
    try {
        encode_speed(std::numeric_limits<double>::infinity());
        std::cerr << "Infinite speed was accepted\n";
        return 1;
    } catch (const std::out_of_range&) {
    }
    if (encode_speed(0) != std::vector<std::uint8_t>{0, 0, 0, 0} ||
        decode_rpm(encode_rpm(65535).data(), 2) != 65535 ||
        decode_temperature(encode_temperature(215).data(), 2) != 215) {
        std::cerr << "Boundary value mismatch\n";
        return 1;
    }
    const vehicle_service::VehicleData sample{123.45, 2500, 85, {}};
    const std::vector<std::uint8_t> vehicle{
        0x00, 0x00, 0x30, 0x39, 0x09, 0xC4, 0x00, 0x55};
    if (encode_vehicle_data(sample) != vehicle) {
        std::cerr << "Incorrect VehicleData event encoding\n";
        return 1;
    }
    const auto decoded = decode_vehicle_data(vehicle.data(), vehicle.size());
    if (!decoded || decoded->vehicle_speed_kph != 123.45 ||
        decoded->engine_rpm != 2500 || decoded->coolant_temperature_c != 85 ||
        decode_vehicle_data(vehicle.data(), 7)) {
        std::cerr << "Incorrect VehicleData event decoding\n";
        return 1;
    }
    const vehicle_service::BodyStatus body{0x11, {}};
    const auto body_bytes = encode_body_status(body);
    const auto decoded_body = decode_body_status(body_bytes.data(), body_bytes.size());
    if (body_bytes != std::vector<std::uint8_t>{0x11} || !decoded_body ||
        decoded_body->flags != 0x11 || decode_body_status(body_bytes.data(), 0)) {
        std::cerr << "Incorrect BodyStatus payload coding\n";
        return 1;
    }
    return 0;
}
