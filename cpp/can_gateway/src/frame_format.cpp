#include "can_gateway/frame_format.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace can_gateway {

std::string format_frame(const CanFrame& frame) {
    const auto time = std::chrono::system_clock::to_time_t(frame.received_at);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  frame.received_at.time_since_epoch()) %
                              1000;
    std::tm utc{};
    gmtime_r(&time, &utc);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.'
           << std::setfill('0') << std::setw(3) << milliseconds.count()
           << "Z ID=0x" << std::uppercase << std::hex << std::setw(3)
           << frame.id << std::dec << " DLC="
           << static_cast<unsigned int>(frame.dlc) << " DATA=";

    for (unsigned int i = 0; i < frame.dlc && i < frame.payload.size(); ++i) {
        if (i != 0) {
            output << ' ';
        }
        output << std::uppercase << std::hex << std::setw(2)
               << static_cast<unsigned int>(frame.payload[i]);
    }
    return output.str();
}

}  // namespace can_gateway
