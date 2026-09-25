#include "vehicle_service/vehicle_service.hpp"

namespace vehicle_service {

void VehicleService::update(VehicleData data) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = data;
}

void VehicleService::updateBody(BodyStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    body_status_ = status;
}

std::optional<BodyStatus> VehicleService::bodyStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return body_status_;
}

std::optional<VehicleData> VehicleService::latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

std::optional<double> VehicleService::getVehicleSpeed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_ ? std::optional<double>(latest_->vehicle_speed_kph) : std::nullopt;
}

std::optional<std::uint16_t> VehicleService::getEngineRpm() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_ ? std::optional<std::uint16_t>(latest_->engine_rpm) : std::nullopt;
}

std::optional<std::int16_t> VehicleService::getCoolantTemperature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_ ? std::optional<std::int16_t>(latest_->coolant_temperature_c) : std::nullopt;
}

}  // namespace vehicle_service
