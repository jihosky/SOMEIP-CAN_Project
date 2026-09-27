#pragma once

#include "vehicle_service/vehicle_service.hpp"

#include <memory>
#include <functional>
#include <vsomeip/vsomeip.hpp>

namespace vehicle_someip {

class VehicleDataProvider {
public:
    VehicleDataProvider(std::shared_ptr<vsomeip::application> application,
                        const vehicle_service::VehicleService& service,
                        std::function<bool(std::uint8_t, bool)> send_door_command = {});
    bool init();
    void start();
    void publish(const vehicle_service::VehicleData& data);
    void publishBody(const vehicle_service::BodyStatus& status);

private:
    void on_request(const std::shared_ptr<vsomeip::message>& request);

    std::shared_ptr<vsomeip::application> application_;
    const vehicle_service::VehicleService& service_;
    std::function<bool(std::uint8_t, bool)> send_door_command_;
};

}  // namespace vehicle_someip
