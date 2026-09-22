#include "can_gateway/frame_format.hpp"

#include <chrono>
#include <iostream>
#include <string>

int main() {
    can_frame frame{};
    frame.can_id = 0x100;
    frame.can_dlc = 8;
    for (unsigned int i = 0; i < 8; ++i) {
        frame.data[i] = static_cast<unsigned char>(i);
    }
    const auto received_at = std::chrono::system_clock::time_point{};
    const std::string expected =
        "1970-01-01T00:00:00.000Z ID=0x100 DLC=8 DATA=00 01 02 03 04 05 06 07";
    if (can_gateway::format_frame(frame, received_at) != expected) {
        std::cerr << "Unexpected CAN frame format\n";
        return 1;
    }
    return 0;
}
