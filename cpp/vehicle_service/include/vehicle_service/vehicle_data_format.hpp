#pragma once

#include "vehicle_service/vehicle_data.hpp"

#include <string>

namespace vehicle_service {

std::string format_vehicle_data(const VehicleData& data);

}  // namespace vehicle_service
