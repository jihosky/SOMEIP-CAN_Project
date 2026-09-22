#include "vehicle_service/vehicle_data_format.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace vehicle_service {

std::string format_vehicle_data(const VehicleData& data) {
    const auto time = std::chrono::system_clock::to_time_t(data.timestamp);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  data.timestamp.time_since_epoch()) %
                              1000;
    std::tm utc{};
    gmtime_r(&time, &utc);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.'
           << std::setfill('0') << std::setw(3) << milliseconds.count()
           << "Z vehicle_speed_kph=" << std::fixed << std::setprecision(2)
           << data.vehicle_speed_kph << " engine_rpm=" << data.engine_rpm
           << " coolant_temperature_c=" << data.coolant_temperature_c;
    return output.str();
}

}  // namespace vehicle_service
