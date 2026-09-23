#pragma once

#include "vehicle_service/vehicle_service.hpp"

#include <memory>
#include <vsomeip/vsomeip.hpp>

namespace vehicle_someip {

class VehicleDataProvider {
public:
    VehicleDataProvider(std::shared_ptr<vsomeip::application> application,
                        const vehicle_service::VehicleService& service);
    bool init();
    void start();

private:
    void on_request(const std::shared_ptr<vsomeip::message>& request);

    std::shared_ptr<vsomeip::application> application_;
    const vehicle_service::VehicleService& service_;
};

}  // namespace vehicle_someip
