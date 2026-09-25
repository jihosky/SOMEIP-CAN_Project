#include "vehicle_someip/vehicle_data_provider.hpp"

#include "vehicle_someip/identifiers.hpp"
#include "vehicle_someip/payload_codec.hpp"

#include <chrono>
#include <iostream>
#include <set>
#include <vector>

namespace vehicle_someip {

VehicleDataProvider::VehicleDataProvider(
    std::shared_ptr<vsomeip::application> application,
    const vehicle_service::VehicleService& service,
    std::function<bool(std::uint8_t, bool)> send_door_command)
    : application_(std::move(application)), service_(service),
      send_door_command_(std::move(send_door_command)) {}

bool VehicleDataProvider::init() {
    if (!application_ || !application_->init()) return false;
    for (const auto method : {get_vehicle_speed_id, get_engine_rpm_id,
                              get_coolant_temperature_id, get_body_status_id,
                              set_door_id}) {
        application_->register_message_handler(
            service_id, instance_id, method,
            [this](const std::shared_ptr<vsomeip::message>& request) {
                on_request(request);
            });
    }
    application_->register_state_handler([this](vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            std::cerr << "[SOMEIP] vehicle-provider registered; offering VehicleData event"
                         " 0x8001/group 0x0001 and service 0x6301/0x0001" << std::endl;
            application_->offer_event(
                service_id, instance_id, vehicle_data_event_id,
                {vehicle_data_eventgroup_id}, vsomeip::event_type_e::ET_EVENT,
                std::chrono::milliseconds::zero(), false, true, nullptr,
                vsomeip::reliability_type_e::RT_RELIABLE);
            application_->offer_event(
                service_id, instance_id, body_status_event_id,
                {body_status_eventgroup_id}, vsomeip::event_type_e::ET_EVENT,
                std::chrono::milliseconds::zero(), false, true, nullptr,
                vsomeip::reliability_type_e::RT_RELIABLE);
            application_->offer_service(service_id, instance_id);
            std::cerr << "[SOMEIP] offer_service requested; confirm SD OfferService"
                         " on UDP 30490 and TCP 30540 listener" << std::endl;
        } else {
            std::cerr << "[SOMEIP] vehicle-provider deregistered" << std::endl;
        }
    });
    return true;
}

void VehicleDataProvider::start() { application_->start(); }

void VehicleDataProvider::publish(const vehicle_service::VehicleData& data) {
    const auto bytes = encode_vehicle_data(data);
    application_->notify(service_id, instance_id, vehicle_data_event_id,
                         vsomeip::runtime::get()->create_payload(bytes), true);
}

void VehicleDataProvider::publishBody(const vehicle_service::BodyStatus& status) {
    application_->notify(service_id, instance_id, body_status_event_id,
                         vsomeip::runtime::get()->create_payload(
                             encode_body_status(status)), true);
}

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
    case get_body_status_id:
        if (const auto status = service_.bodyStatus()) {
            bytes = encode_body_status(*status);
            available = true;
        }
        break;
    case set_door_id: {
        const auto payload = request->get_payload();
        if (!payload || payload->get_length() != 2 || !payload->get_data() ||
            payload->get_data()[0] > 4 || payload->get_data()[1] > 1) {
            response->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
            application_->send(response);
            return;
        }
        if (!send_door_command_) {
            response->set_return_code(vsomeip::return_code_e::E_NOT_READY);
            application_->send(response);
            return;
        }
        if (!send_door_command_(payload->get_data()[0], payload->get_data()[1] == 1)) {
            response->set_return_code(vsomeip::return_code_e::E_NOT_READY);
            application_->send(response);
            return;
        }
        bytes = {1}; // Accepted for vCAN transmission; status event confirms state.
        available = true;
        break;
    }
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
