#include "can_gateway/body_command_sender.hpp"

#include "body_definition.hpp"

#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

namespace can_gateway {

BodyCommandSender::~BodyCommandSender() {
    if (socket_fd_ >= 0) close(socket_fd_);
}

bool BodyCommandSender::open(const std::string& interface_name, std::string& error) {
    // This demo's remote commands must never address a physical CAN interface.
    if (interface_name.rfind("vcan", 0) != 0) {
        error = "Body commands require a vcan interface";
        return false;
    }
    const auto index = if_nametoindex(interface_name.c_str());
    if (index == 0) {
        error = "Unknown CAN interface: " + interface_name;
        return false;
    }
    socket_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd_ < 0) {
        error = std::string("Body command socket: ") + std::strerror(errno);
        return false;
    }
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = static_cast<int>(index);
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        error = std::string("Bind body command socket: ") + std::strerror(errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    return true;
}

bool BodyCommandSender::send(std::uint8_t door, bool open, std::string& error) const {
    if (socket_fd_ < 0 || door > 4) {
        error = "Invalid body command or sender not open";
        return false;
    }
    can_frame frame{};
    frame.can_id = body_definition::command_can_id;
    frame.can_dlc = body_definition::command_dlc;
    frame.data[0] = door;
    frame.data[1] = open ? 1 : 0;
    if (write(socket_fd_, &frame, sizeof(frame)) != sizeof(frame)) {
        error = std::string("Send body command: ") + std::strerror(errno);
        return false;
    }
    return true;
}

}  // namespace can_gateway
