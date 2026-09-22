#pragma once

#include <array>
#include <chrono>
#include <cstdint>

namespace can_gateway {

struct CanFrame {
    std::uint32_t id{};
    std::uint8_t dlc{};
    std::array<std::uint8_t, 8> payload{};
    std::chrono::system_clock::time_point received_at{};
};

}  // namespace can_gateway
