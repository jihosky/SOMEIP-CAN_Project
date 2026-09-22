#include "can_gateway/frame_format.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace can_gateway {

std::string format_frame(const can_frame& frame,
                         std::chrono::system_clock::time_point received_at) {
    const auto time = std::chrono::system_clock::to_time_t(received_at);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  received_at.time_since_epoch()) %
                              1000;
    std::tm utc{};
    gmtime_r(&time, &utc);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.'
           << std::setfill('0') << std::setw(3) << milliseconds.count()
           << "Z ID=0x" << std::uppercase << std::hex << std::setw(3)
           << (frame.can_id & CAN_SFF_MASK) << std::dec << " DLC="
           << static_cast<unsigned int>(frame.can_dlc) << " DATA=";

    for (unsigned int i = 0; i < frame.can_dlc && i < CAN_MAX_DLEN; ++i) {
        if (i != 0) {
            output << ' ';
        }
        output << std::uppercase << std::hex << std::setw(2)
               << static_cast<unsigned int>(frame.data[i]);
    }
    return output.str();
}

}  // namespace can_gateway
