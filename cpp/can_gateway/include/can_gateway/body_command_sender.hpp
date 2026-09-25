#pragma once

#include <cstdint>
#include <string>

namespace can_gateway {

class BodyCommandSender {
public:
    BodyCommandSender() = default;
    BodyCommandSender(const BodyCommandSender&) = delete;
    BodyCommandSender& operator=(const BodyCommandSender&) = delete;
    ~BodyCommandSender();

    bool open(const std::string& interface_name, std::string& error);
    bool send(std::uint8_t door, bool open, std::string& error) const;

private:
    int socket_fd_ = -1;
};

}  // namespace can_gateway
