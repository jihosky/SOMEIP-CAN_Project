#include "can_gateway/frame_format.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t running = 1;

void stop(int) { running = 0; }

int run(const std::string& interface_name) {
    if (interface_name.empty() || interface_name.size() >= IFNAMSIZ) {
        std::cerr << "Invalid CAN interface name\n";
        return 2;
    }

    const int socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd < 0) {
        std::cerr << "SocketCAN socket: " << std::strerror(errno) << '\n';
        return 1;
    }

    const unsigned int interface_index = if_nametoindex(interface_name.c_str());
    if (interface_index == 0) {
        std::cerr << "CAN interface '" << interface_name << "': "
                  << std::strerror(errno) << '\n';
        close(socket_fd);
        return 1;
    }

    const timeval timeout{1, 0};
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "SocketCAN timeout: " << std::strerror(errno) << '\n';
        close(socket_fd);
        return 1;
    }

    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = static_cast<int>(interface_index);
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Bind CAN interface '" << interface_name << "': "
                  << std::strerror(errno) << '\n';
        close(socket_fd);
        return 1;
    }

    while (running) {
        can_frame frame{};
        const auto bytes = recv(socket_fd, &frame, sizeof(frame), 0);
        if (bytes < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            std::cerr << "SocketCAN receive: " << std::strerror(errno) << '\n';
            close(socket_fd);
            return 1;
        }
        if (bytes != sizeof(frame)) {
            std::cerr << "SocketCAN receive: incomplete CAN frame\n";
            close(socket_fd);
            return 1;
        }
        if ((frame.can_id & (CAN_EFF_FLAG | CAN_ERR_FLAG | CAN_RTR_FLAG)) != 0) {
            continue;
        }
        std::cout << can_gateway::format_frame(frame, std::chrono::system_clock::now())
                  << std::endl;
    }

    close(socket_fd);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string interface_name = "vcan0";
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " [--interface NAME]\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--interface") {
        interface_name = argv[2];
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [--interface NAME]\n";
        return 2;
    }

    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    return run(interface_name);
}
