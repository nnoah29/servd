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
#include <sstream>

namespace servd
{
    Server::UringEngine::UringTcpConnection::UringTcpConnection(int fd, UringEngine& engine)
        : fd_(fd), engine_(engine) {}

    Task<void> Server::UringEngine::UringTcpConnection::send_frame(
        const FrameHeader& header, std::span<const std::byte> payload)
    {
        std::vector<std::byte> buffer(sizeof(FrameHeader) + payload.size());
        std::memcpy(buffer.data(), &header, sizeof(FrameHeader));
        if (!payload.empty()) {
            std::memcpy(buffer.data() + sizeof(FrameHeader), payload.data(), payload.size());
        }
        co_await engine_.async_write(fd_, buffer);
    }

    Server::UringEngine::TextTcpConnection::TextTcpConnection(int fd, UringEngine& engine)
        : fd_(fd), engine_(engine) {}

    Task<void> Server::UringEngine::TextTcpConnection::send_frame(
        const FrameHeader& header, std::span<const std::byte> payload)
    {
        std::ostringstream oss;
        oss << header.command_id << ' ' << header.flags << ' ' << header.session_id << '\n';

        if (!payload.empty()) {
            const auto* chars = reinterpret_cast<const char*>(payload.data());
            oss.write(chars, static_cast<std::streamsize>(payload.size()));
        }
        oss << '\n';

        const std::string text = std::move(oss).str();
        const auto* data = reinterpret_cast<const std::byte*>(text.data());
        co_await engine_.async_write(fd_, {data, text.size()});
    }

    Server::UringEngine::UringUdpConnection::UringUdpConnection(
        int fd, UringEngine& engine, const struct sockaddr_storage& addr)
        : fd_(fd), engine_(engine), client_addr_(addr) {}
}
