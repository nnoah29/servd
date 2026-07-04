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

#include "detail/Engine.hpp"
#include <Logger.hpp>
#include <cstring>
#include <vector>

namespace servd
{

    Task<void> Server::UringEngine::send_to_session(
        uint64_t session_id, uint16_t command_id, bytes payload)
    {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) co_return;
        FrameHeader header{command_id, 0, static_cast<uint32_t>(payload.size()), session_id};
        std::vector<std::byte> buffer(sizeof(FrameHeader) + payload.size());
        std::memcpy(buffer.data(), &header, sizeof(FrameHeader));
        if (!payload.empty()) {
            std::memcpy(buffer.data() + sizeof(FrameHeader), payload.data(), payload.size());
        }
        co_await async_write(it->second, buffer);
    }

    Task<void> Server::UringEngine::do_broadcast(uint16_t command_id, bytes payload) {
        auto sessions_snapshot = sessions_;
        for (const auto& [sid, fd] : sessions_snapshot) {
            FrameHeader header{command_id, 0, static_cast<uint32_t>(payload.size()), sid};
            std::vector<std::byte> buffer(sizeof(FrameHeader) + payload.size());
            std::memcpy(buffer.data(), &header, sizeof(FrameHeader));
            if (!payload.empty()) {
                std::memcpy(buffer.data() + sizeof(FrameHeader), payload.data(), payload.size());
            }
            co_await async_write(fd, buffer);
        }
    }

    Task<void> Server::UringEngine::do_broadcast_if(uint16_t command_id, bytes payload,
        std::function<bool(const Session&)> predicate) {
        auto sessions_snapshot = sessions_;
        for (const auto& [sid, fd] : sessions_snapshot) {
            if (!server_.session_store_) continue;
            Session session = co_await server_.session_store_->get_or_create(sid);
            if (!predicate(session)) continue;
            FrameHeader header{command_id, 0, static_cast<uint32_t>(payload.size()), sid};
            std::vector<std::byte> buffer(sizeof(FrameHeader) + payload.size());
            std::memcpy(buffer.data(), &header, sizeof(FrameHeader));
            if (!payload.empty()) {
                std::memcpy(buffer.data() + sizeof(FrameHeader), payload.data(), payload.size());
            }
            co_await async_write(fd, buffer);
        }
    }

}
