#include "vehicle_service/vehicle_service.hpp"

namespace vehicle_service {

void VehicleService::update(VehicleData data) { latest_ = data; }

std::optional<VehicleData> VehicleService::latest() const { return latest_; }

std::optional<double> VehicleService::getVehicleSpeed() const {
    return latest_ ? std::optional<double>(latest_->vehicle_speed_kph) : std::nullopt;
}

std::optional<std::uint16_t> VehicleService::getEngineRpm() const {
    return latest_ ? std::optional<std::uint16_t>(latest_->engine_rpm) : std::nullopt;
}

std::optional<std::int16_t> VehicleService::getCoolantTemperature() const {
    return latest_ ? std::optional<std::int16_t>(latest_->coolant_temperature_c) : std::nullopt;
}

}  // namespace vehicle_service
