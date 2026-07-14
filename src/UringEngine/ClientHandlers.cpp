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
#include <vector>
#include <array>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/time_types.h>
#include <systemd/sd-bus.h>

namespace servd
{
    DetachedTask Server::UringEngine::handle_client(int client_fd)
    {
        ++active_connections_;
        UringTcpConnection connection(client_fd, *this);
        auto current_sid = static_cast<uint64_t>(-1);

        while (running) {
            try {
                ClientFrame frame = co_await read_frame(client_fd);

                if (!server_.session_store_)
                    throw std::runtime_error("SessionStore not initialized");

                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);

                if (frame.header.session_id != current_sid) {
                    if (current_sid != 0) unregister_session(current_sid);
                    register_session(frame.header.session_id, client_fd);
                    current_sid = frame.header.session_id;
                }

                co_await process_command(frame, connection, session);
                co_await server_.session_store_->save(session);

            } catch (const std::exception& e) {
                SERVD_LOG(Logger::LogLevel::WARN, "[Disconnect/Error] Client %d: %s", client_fd, e.what());
                break;
            }
        }

        SERVD_LOG(Logger::LogLevel::INFO, "[Client] Client %d disconnected", client_fd);

        if (current_sid != static_cast<uint64_t>(-1))
            unregister_session(current_sid);

        if (server_.disconnect_handler_ && current_sid != static_cast<uint64_t>(-1)) {
            co_await server_.thread_pool().enqueue([sid = current_sid, &srv = server_]() {
                srv.disconnect_handler_(sid);
                return 0;
            });
        }

        close(client_fd);
        --active_connections_;
        if (server_.max_clients_ > 0 && active_connections_ < server_.max_clients_
            && !server_.discovery_config_.respond_to_clients) {
            server_.discovery_config_.respond_to_clients = true;
            server_.reenable_discovery();
            SERVD_LOG(Logger::LogLevel::INFO, "[Discovery] Re-enabled after disconnect");
        }
    }

    DetachedTask Server::UringEngine::text_handle_client(int client_fd)
    {
        ++active_connections_;
        TextTcpConnection connection(client_fd, *this);
        auto current_sid = static_cast<uint64_t>(-1);

        while (running) {
            try {
                ClientFrame frame = co_await read_text_frame(client_fd);

                if (!server_.session_store_)
                    throw std::runtime_error("SessionStore not initialized");

                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);

                if (frame.header.session_id != current_sid) {
                    if (current_sid != 0) unregister_session(current_sid);
                    register_session(frame.header.session_id, client_fd);
                    current_sid = frame.header.session_id;
                }

                co_await process_command(frame, connection, session);
                co_await server_.session_store_->save(session);
            } catch (const std::exception& e) {
                SERVD_LOG(Logger::LogLevel::WARN, "[Disconnect/Error] Client %d: %s", client_fd, e.what());
                break;
            }
        }

        SERVD_LOG(Logger::LogLevel::INFO, "[Client] Client %d disconnected", client_fd);

        if (current_sid != static_cast<uint64_t>(-1))
            unregister_session(current_sid);
        close(client_fd);
        --active_connections_;
        if (server_.max_clients_ > 0 && active_connections_ < server_.max_clients_
            && !server_.discovery_config_.respond_to_clients) {
            server_.discovery_config_.respond_to_clients = true;
            server_.reenable_discovery();
            SERVD_LOG(Logger::LogLevel::INFO, "[Discovery] Re-enabled after disconnect");
        }
    }

    DetachedTask Server::UringEngine::start_accept_loop(int server_fd, ProtocolMode mode)
    {
        while (running) {
            try {

                const int client_fd = co_await async_accept(server_fd);
                if (server_.max_clients_ > 0 && active_connections_ >= server_.max_clients_) {
                    SERVD_LOG(Logger::LogLevel::WARN, "[Reject] Max client limit reached (%zu)", server_.max_clients_);
                    server_.discovery_config_.respond_to_clients = false;
                    server_.disable_discovery();
                    close(client_fd);
                    continue;
                }

                SERVD_LOG(Logger::LogLevel::INFO, "[New Client] FD connected: %d", client_fd);

                if (mode == ProtocolMode::TEXT)
                    text_handle_client(client_fd);
                else
                    handle_client(client_fd);

            } catch (std::exception& e) {
                SERVD_LOG(Logger::LogLevel::ERROR, "[Error] %s", e.what());
            }
        }
    }

    DetachedTask Server::UringEngine::start_udp_loop(int udp_fd)
    {
        std::array<std::byte, 65536> buffer{};

        while (running) {
            try {
                struct sockaddr_storage client_addr{};
                const size_t bytes = co_await async_recvmsg(udp_fd, buffer, client_addr);

                if (bytes < sizeof(FrameHeader)) continue;

                FrameHeader header{};
                std::memcpy(&header, buffer.data(), sizeof(FrameHeader));
                const uint32_t pl = std::min(header.payload_length,
                    static_cast<uint32_t>(bytes - sizeof(FrameHeader)));

                header.payload_length = pl;
                std::vector<std::byte> payload(pl);

                if (pl)
                    std::memcpy(payload.data(), buffer.data() + sizeof(FrameHeader), pl);
                UringUdpConnection conn(udp_fd, *this, client_addr);
                if (!server_.session_store_)
                    continue;
                Session session = co_await server_.session_store_->get_or_create(header.session_id);
                co_await process_command({header, std::move(payload)}, conn, session);
                co_await server_.session_store_->save(session);

            } catch (const std::exception& e) {
                SERVD_LOG(Logger::LogLevel::ERROR, "[UDP Error] %s", e.what());
            }
        }
    }

    DetachedTask Server::UringEngine::start_discovery_loop(int udp_fd)
    {
        while (running) {
            try {
                struct sockaddr_storage client_addr{};
                DiscoveryPacket req{};
                const size_t bytes = co_await async_recvmsg(udp_fd,
                    {reinterpret_cast<std::byte*>(&req), sizeof(req)}, client_addr);

                if (bytes < sizeof(DiscoveryPacket)) continue;
                if (req.magic_number != server_.discovery_config_.magic_number) continue;
                if (req.action != DiscoveryAction::CLIENT_LOOKING_FOR_SERVER) continue;
                if (!server_.discovery_config_.respond_to_clients) continue;

                const auto& sin = reinterpret_cast<const struct sockaddr_in&>(client_addr);
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sin.sin_addr, ip_str, sizeof(ip_str));
                SERVD_LOG(Logger::LogLevel::INFO, "[Discovery] Request from %s:%d",
                    ip_str, ntohs(sin.sin_port));

                DiscoveryPacket resp{};
                resp.magic_number = server_.discovery_config_.magic_number;
                resp.action = DiscoveryAction::SERVER_ANNOUNCING;
                resp.tcp_port = server_.discovery_config_.advertised_tcp_port
                    ? server_.discovery_config_.advertised_tcp_port
                    : (server_.tcp_ports_.empty() ? 0 : server_.tcp_ports_[0].first);
                resp.udp_port = server_.discovery_config_.advertised_udp_port
                    ? server_.discovery_config_.advertised_udp_port
                    : (server_.udp_ports_.empty() ? 0 : server_.udp_ports_[0]);

                if (server_.discovery_config_.on_discovery_request)
                    server_.discovery_config_.on_discovery_request(sin, resp);

                co_await async_sendto(udp_fd,
                    {reinterpret_cast<std::byte*>(&resp), sizeof(resp)}, client_addr);

            } catch (const std::exception& e) {
                SERVD_LOG(Logger::LogLevel::ERROR, "[Discovery] Error: %s", e.what());
            }
        }
    }

    DetachedTask Server::UringEngine::bus_monitor_loop(sd_bus* bus)
    {
        const int fd = sd_bus_get_fd(bus);
        while (running) {
            const int mask = co_await async_poll_add(fd, POLLIN);
            if (mask < 0) continue;
            while (sd_bus_process(bus, nullptr) > 0) {}
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
                SERVD_LOG(Logger::LogLevel::ERROR, "[Timer] Error: %s", e.what());
            }
        }
    }

}
