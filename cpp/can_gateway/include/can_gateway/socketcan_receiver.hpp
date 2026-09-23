#pragma once

#include "can_gateway/can_frame.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace can_gateway {

class SocketCanReceiver {
public:
    SocketCanReceiver() = default;
    SocketCanReceiver(const SocketCanReceiver&) = delete;
    SocketCanReceiver& operator=(const SocketCanReceiver&) = delete;
    ~SocketCanReceiver();

    bool open(const std::string& interface_name, std::string& error);
    bool run(const std::function<void(const CanFrame&)>& on_frame, std::string& error);
    void stop() noexcept;

private:
    int socket_fd_ = -1;
    std::atomic<bool> running_{false};
};

}  // namespace can_gateway
