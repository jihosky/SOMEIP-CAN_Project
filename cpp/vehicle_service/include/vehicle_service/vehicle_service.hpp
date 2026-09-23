#pragma once

#include "vehicle_service/vehicle_data.hpp"

#include <optional>
#include <mutex>

namespace vehicle_service {

class VehicleService {
public:
    void update(VehicleData data);
    std::optional<VehicleData> latest() const;
    std::optional<double> getVehicleSpeed() const;
    std::optional<std::uint16_t> getEngineRpm() const;
    std::optional<std::int16_t> getCoolantTemperature() const;

private:
    mutable std::mutex mutex_;
    std::optional<VehicleData> latest_;
};

}  // namespace vehicle_service
