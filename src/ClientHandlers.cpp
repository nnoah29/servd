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
#include <array>
#include <chrono>
#include <unistd.h>
#include <linux/time_types.h>

namespace servd
{

    DetachedTask Server::UringEngine::handle_client(int client_fd)
    {
        ++active_connections_;
        UringTcpConnection connection(client_fd, *this);
        uint64_t current_sid = static_cast<uint64_t>(-1);
        while (running) {
            try {
                ClientFrame frame = co_await read_frame(client_fd);
                if (!server_.session_store_)
                    throw std::runtime_error("SessionStore non initialise");
                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);
                if (frame.header.session_id != current_sid) {
                    if (current_sid != 0) unregister_session(current_sid);
                    register_session(frame.header.session_id, client_fd);
                    current_sid = frame.header.session_id;
                }
                co_await process_command(frame, connection, session);
                co_await server_.session_store_->save(session);
            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::WARN, "[Deconnexion/Erreur] Client %d : %s", client_fd, e.what());
                break;
            }
        }
        if (current_sid != static_cast<uint64_t>(-1)) unregister_session(current_sid);
        close(client_fd);
        --active_connections_;
    }

    DetachedTask Server::UringEngine::text_handle_client(int client_fd)
    {
        ++active_connections_;
        TextTcpConnection connection(client_fd, *this);
        uint64_t current_sid = static_cast<uint64_t>(-1);
        while (running) {
            try {
                ClientFrame frame = co_await read_text_frame(client_fd);
                if (!server_.session_store_)
                    throw std::runtime_error("SessionStore non initialise");
                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);
                if (frame.header.session_id != current_sid) {
                    if (current_sid != 0) unregister_session(current_sid);
                    register_session(frame.header.session_id, client_fd);
                    current_sid = frame.header.session_id;
                }
                co_await process_command(frame, connection, session);
                co_await server_.session_store_->save(session);
            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::WARN, "[Deconnexion/Erreur] Client %d : %s", client_fd, e.what());
                break;
            }
        }
        if (current_sid != static_cast<uint64_t>(-1)) unregister_session(current_sid);
        close(client_fd);
        --active_connections_;
    }

    DetachedTask Server::UringEngine::start_accept_loop(int server_fd, ProtocolMode mode)
    {
        while (running) {
            try {
                const int client_fd = co_await async_accept(server_fd);
                if (server_.max_clients_ > 0 && active_connections_ >= server_.max_clients_) {
                    LOG(Logger::LogLevel::WARN, "[Rejet] Limite de clients atteinte (%zu)", server_.max_clients_);
                    close(client_fd);
                    continue;
                }
                LOG(Logger::LogLevel::INFO, "[Nouveau Client] FD connecte : %d", client_fd);
                if (mode == ProtocolMode::TEXT) {
                    text_handle_client(client_fd);
                } else {
                    handle_client(client_fd);
                }
            } catch (std::exception& e) {
                LOG(Logger::LogLevel::ERROR, "[Erreur] %s", e.what());
            }
        }
    }

    DetachedTask Server::UringEngine::start_udp_loop(int udp_fd)
    {
        std::array<std::byte, 65536> buffer{};
        while (running) {
            try {
                struct sockaddr_storage client_addr{};
                size_t bytes = co_await async_recvmsg(udp_fd, buffer, client_addr);
                if (bytes < sizeof(FrameHeader)) continue;
                FrameHeader header{};
                std::memcpy(&header, buffer.data(), sizeof(FrameHeader));
                uint32_t pl = std::min(header.payload_length,
                    static_cast<uint32_t>(bytes - sizeof(FrameHeader)));
                header.payload_length = pl;
                std::vector<std::byte> payload(pl);
                if (pl) std::memcpy(payload.data(), buffer.data() + sizeof(FrameHeader), pl);
                UringUdpConnection conn(udp_fd, *this, client_addr);
                if (!server_.session_store_) continue;
                Session session = co_await server_.session_store_->get_or_create(header.session_id);
                co_await process_command({header, std::move(payload)}, conn, session);
                co_await server_.session_store_->save(session);
            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::ERROR, "[Erreur UDP] %s", e.what());
            }
        }
    }

    DetachedTask Server::UringEngine::periodic_timer_loop(
        std::chrono::milliseconds interval, PeriodicTaskHandler handler)
    {
        struct __kernel_timespec ts{};
        ts.tv_sec = interval.count() / 1000;
        ts.tv_nsec = (interval.count() % 1000) * 1000000;
        while (running) {
            UringOperation op;
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            io_uring_prep_timeout(sqe, &ts, 0, 0);
            io_uring_sqe_set_data(sqe, &op);
            io_uring_submit(&ring);
            (void)co_await op;
            try {
                co_await handler(server_);
            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::ERROR, "[Timer] Erreur: %s", e.what());
            }
        }
    }

}
