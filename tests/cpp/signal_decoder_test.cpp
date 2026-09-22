#include "can_gateway/signal_decoder.hpp"
#include "milestone1_definition.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    can_gateway::SignalDecoder decoder;
    can_gateway::CanFrame frame{};
    frame.id = milestone1_definition::can_id;
    frame.dlc = milestone1_definition::dlc;
    frame.payload = {0x39, 0x30, 0xC4, 0x09, 0x7D, 0, 0, 0};
    frame.received_at = std::chrono::system_clock::time_point{std::chrono::seconds{42}};

    const auto data = decoder.decode(frame);
    if (!check(data.has_value(), "Expected milestone frame to decode")) return 1;
    if (!check(std::abs(data->vehicle_speed_kph - 123.45) < 0.0001,
               "Incorrect speed")) return 1;
    if (!check(data->engine_rpm == 2500, "Incorrect RPM")) return 1;
    if (!check(data->coolant_temperature_c == 85, "Incorrect temperature")) return 1;
    if (!check(data->timestamp == frame.received_at, "Timestamp not preserved")) return 1;

    frame.payload = {0, 0, 0, 0, 0, 0, 0, 0};
    const auto minimum = decoder.decode(frame);
    if (!check(minimum && minimum->vehicle_speed_kph == 0 && minimum->engine_rpm == 0 &&
                   minimum->coolant_temperature_c == -40,
               "Incorrect minimum values")) return 1;

    frame.payload = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0};
    const auto maximum = decoder.decode(frame);
    if (!check(maximum && std::abs(maximum->vehicle_speed_kph - 655.35) < 0.0001 &&
                   maximum->engine_rpm == 65535 &&
                   maximum->coolant_temperature_c == 215,
               "Incorrect maximum values")) return 1;

    frame.dlc = 4;
    if (!check(!decoder.decode(frame), "Short frame must be ignored")) return 1;
    frame.dlc = milestone1_definition::dlc;
    frame.id = 0x101;
    if (!check(!decoder.decode(frame), "Other CAN ID must be ignored")) return 1;
    return 0;
}
