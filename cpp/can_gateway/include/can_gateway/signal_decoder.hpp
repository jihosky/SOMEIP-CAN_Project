#pragma once

#include "can_gateway/can_frame.hpp"
#include "vehicle_service/vehicle_data.hpp"
#include "vehicle_service/body_status.hpp"

#include <optional>

namespace can_gateway {

class SignalDecoder {
public:
    std::optional<vehicle_service::VehicleData> decode(const CanFrame& frame) const;
    std::optional<vehicle_service::BodyStatus> decode_body(const CanFrame& frame) const;
};

}  // namespace can_gateway
