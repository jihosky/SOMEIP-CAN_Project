#pragma once

#include <chrono>
#include <linux/can.h>
#include <string>

namespace can_gateway {

std::string format_frame(const can_frame& frame,
                         std::chrono::system_clock::time_point received_at);

}  // namespace can_gateway
