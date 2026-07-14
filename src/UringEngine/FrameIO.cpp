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
#include <servd/Logger.hpp>
#include <cstring>
#include <charconv>
#include <sstream>

namespace servd {

    namespace {

        bool parse_text_header(const std::string& header_line, uint16_t& cmd, uint64_t& sid,
            const std::unordered_map<std::string, uint16_t>& cmd_names)
        {
            std::istringstream iss(header_line);
            std::string token;
            iss >> token;
            auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), cmd);

            if (ec != std::errc()) {
                const auto it = cmd_names.find(token);
                if (it != cmd_names.end()) { cmd = it->second; }
                else { return false; }
            }

            iss >> sid;
            return true;
        }
    }

    Task<ClientFrame> Server::UringEngine::read_frame(int client_fd)
    {
        ClientFrame frame;
        const std::span<std::byte> header_span{reinterpret_cast<std::byte*>(&frame.header), sizeof(FrameHeader)};

        co_await async_read_exact(client_fd, header_span);
        frame.payload.resize(frame.header.payload_length);

        if (frame.header.payload_length > 0) {
            co_await async_read_exact(client_fd, frame.payload);
        }

        co_return frame;
    }

    Task<std::string> Server::UringEngine::read_text_line(int client_fd)
    {
        std::string line;
        char c;

        while (true) {
            co_await async_read_exact(client_fd, {reinterpret_cast<std::byte*>(&c), 1});
            if (c == '\n') break;
            line += c;
        }

        co_return line;
    }

    Task<ClientFrame> Server::UringEngine::read_text_frame(int client_fd)
    {
        ClientFrame frame{};
        const std::string header_line = co_await read_text_line(client_fd);

        if (!parse_text_header(header_line, frame.header.command_id,
            frame.header.session_id, server_.text_command_names_)) {
            SERVD_LOG(Logger::LogLevel::WARN, "[Text] Invalid header: %s", header_line.c_str());
            co_return frame;
        }

        frame.header.flags = 0;
        const std::string payload_line = co_await read_text_line(client_fd);
        frame.header.payload_length = static_cast<uint32_t>(payload_line.size());
        frame.payload.resize(frame.header.payload_length);

        if (!payload_line.empty()) {
            std::memcpy(frame.payload.data(), payload_line.data(), frame.header.payload_length);
        }
        co_return frame;
    }
}
