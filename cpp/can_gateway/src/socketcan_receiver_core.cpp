#include "can_gateway/socketcan_receiver.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace can_gateway {

SocketCanReceiver::~SocketCanReceiver() {
    if (socket_fd_ >= 0) close(socket_fd_);
}

bool SocketCanReceiver::open(const std::string& interface_name, std::string& error) {
    if (socket_fd_ >= 0) {
        error = "SocketCAN receiver is already open";
        return false;
    }
    if (interface_name.empty() || interface_name.size() >= IFNAMSIZ) {
        error = "Invalid CAN interface name";
        return false;
    }
    socket_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd_ < 0) {
        error = std::string("SocketCAN socket: ") + std::strerror(errno);
        return false;
    }
    const auto index = if_nametoindex(interface_name.c_str());
    if (index == 0) {
        error = "CAN interface '" + interface_name + "': " + std::strerror(errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    const timeval timeout{0, 200000};
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        error = std::string("SocketCAN timeout: ") + std::strerror(errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = static_cast<int>(index);
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        error = "Bind CAN interface '" + interface_name + "': " + std::strerror(errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    running_.store(true);
    return true;
}

bool SocketCanReceiver::run(const std::function<void(const CanFrame&)>& on_frame,
                            std::string& error) {
    if (socket_fd_ < 0) {
        error = "SocketCAN receiver is not open";
        return false;
    }
    while (running_.load()) {
        can_frame socket_frame{};
        const auto bytes = recv(socket_fd_, &socket_frame, sizeof(socket_frame), 0);
        if (bytes < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            error = std::string("SocketCAN receive: ") + std::strerror(errno);
            return false;
        }
        if (bytes != sizeof(socket_frame)) {
            error = "SocketCAN receive: incomplete CAN frame";
            return false;
        }
        if ((socket_frame.can_id & (CAN_EFF_FLAG | CAN_ERR_FLAG | CAN_RTR_FLAG)) != 0) {
            continue;
        }
        CanFrame frame{};
        frame.id = socket_frame.can_id & CAN_SFF_MASK;
        frame.dlc = socket_frame.can_dlc;
        frame.received_at = std::chrono::system_clock::now();
        for (unsigned int i = 0; i < frame.dlc && i < frame.payload.size(); ++i) {
            frame.payload[i] = socket_frame.data[i];
        }
        on_frame(frame);
    }
    return true;
}

void SocketCanReceiver::stop() noexcept { running_.store(false); }

}  // namespace can_gateway
