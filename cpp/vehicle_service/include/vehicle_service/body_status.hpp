#pragma once

#include <chrono>
#include <cstdint>

namespace vehicle_service {

struct BodyStatus {
    std::uint8_t flags{};
    std::chrono::system_clock::time_point timestamp{};
};

}  // namespace vehicle_service
