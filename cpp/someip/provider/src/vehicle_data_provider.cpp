#include "vehicle_someip/vehicle_data_provider.hpp"

#include "vehicle_someip/identifiers.hpp"
#include "vehicle_someip/payload_codec.hpp"

#include <vector>

namespace vehicle_someip {

VehicleDataProvider::VehicleDataProvider(
    std::shared_ptr<vsomeip::application> application,
    const vehicle_service::VehicleService& service)
    : application_(std::move(application)), service_(service) {}

bool VehicleDataProvider::init() {
    if (!application_ || !application_->init()) return false;
    for (const auto method : {get_vehicle_speed_id, get_engine_rpm_id,
                              get_coolant_temperature_id}) {
        application_->register_message_handler(
            service_id, instance_id, method,
            [this](const std::shared_ptr<vsomeip::message>& request) {
                on_request(request);
            });
    }
    application_->register_state_handler([this](vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            application_->offer_service(service_id, instance_id);
        }
    });
    return true;
}

void VehicleDataProvider::start() { application_->start(); }

void VehicleDataProvider::on_request(const std::shared_ptr<vsomeip::message>& request) {
    auto response = vsomeip::runtime::get()->create_response(request);
    std::vector<std::uint8_t> bytes;
    bool available = false;
    switch (request->get_method()) {
    case get_vehicle_speed_id:
        if (const auto value = service_.getVehicleSpeed()) {
            bytes = encode_speed(*value);
            available = true;
        }
        break;
    case get_engine_rpm_id:
        if (const auto value = service_.getEngineRpm()) {
            bytes = encode_rpm(*value);
            available = true;
        }
        break;
    case get_coolant_temperature_id:
        if (const auto value = service_.getCoolantTemperature()) {
            bytes = encode_temperature(*value);
            available = true;
        }
        break;
    default:
        response->set_return_code(vsomeip::return_code_e::E_UNKNOWN_METHOD);
        application_->send(response);
        return;
    }
    if (!available) {
        response->set_return_code(vsomeip::return_code_e::E_NOT_READY);
    } else {
        response->set_payload(vsomeip::runtime::get()->create_payload(bytes));
    }
    application_->send(response);
}

}  // namespace vehicle_someip
