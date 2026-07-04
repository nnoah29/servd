/*
**  _                                              _      ___    ___
** | |                                            | |    |__ \  / _ \
** | |_Created _       _ __   _ __    ___    __ _ | |__     ) || (_) |
** | '_ \ | | | |     | '_ \ | '_ \  / _ \  / _` || '_ \   / /  \__, |
** | |_) || |_| |     | | | || | | || (_) || (_| || | | | / /_    / /
** |_.__/  \__, |     |_| |_||_| |_| \___/  \__,_||_| |_||____|  /_/
**          __/ |     on 30/06/2026.
**         |___/
*/

#include "../detail/Engine.hpp"
#include <cstring>
#include <vector>
#include <string>
#include <arpa/inet.h>

namespace servd
{

    Task<void> Server::UringEngine::UringUdpConnection::send_frame(
        const FrameHeader& header, std::span<const std::byte> payload)
    {
        std::vector<std::byte> buffer(sizeof(FrameHeader) + payload.size());

        std::memcpy(buffer.data(), &header, sizeof(FrameHeader));
        if (!payload.empty()) {
            std::memcpy(buffer.data() + sizeof(FrameHeader), payload.data(), payload.size());
        }
        
        co_await engine_.async_sendto(fd_, buffer, client_addr_);
    }

    std::string Server::UringEngine::UringUdpConnection::get_remote_address() const
    {
        char str[INET_ADDRSTRLEN];
        const auto* sin = reinterpret_cast<const struct sockaddr_in*>(&client_addr_);
        inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str));
        return std::string(str) + ":" + std::to_string(ntohs(sin->sin_port));
    }

}
