#pragma once

#include "can_gateway/can_frame.hpp"

#include <string>

namespace can_gateway {

std::string format_frame(const CanFrame& frame);

}  // namespace can_gateway
