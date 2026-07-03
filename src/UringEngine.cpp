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
#include <arpa/inet.h>
#include <linux/time_types.h>
#include <charconv>

namespace servd
{

    Server::UringEngine::UringEngine(Server& s) : server_(s) {
        if (io_uring_queue_init(256, &ring, 0) < 0)
            throw std::runtime_error("io_uring_queue_init failed");
    }

    Server::UringEngine::~UringEngine() {
        io_uring_queue_exit(&ring);
    }

    void Server::UringEngine::register_session(uint64_t session_id, int fd) {
        sessions_[session_id] = fd;
    }

    void Server::UringEngine::unregister_session(uint64_t session_id) {
        sessions_.erase(session_id);
    }

    Task<void> Server::UringEngine::send_to_session(uint64_t session_id, uint16_t command_id, bytes payload) {
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

    void Server::UringEngine::run() {
        running = true;
        struct io_uring_cqe *cqe;

        while (running) {
            const int res = io_uring_wait_cqe(&ring, &cqe);
            if (res < 0) continue;
            auto *op = static_cast<UringOperation*>(io_uring_cqe_get_data(cqe));

            if (op != nullptr) {
                op->cqe_res = cqe->res;
                if (op->coroutine)
                    op->coroutine.resume();
            }
            io_uring_cqe_seen(&ring, cqe);
        }
    }

    Task<int> Server::UringEngine::async_accept(int server_fd)
    {
        UringOperation op;

        struct sockaddr_storage client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_accept(sqe, server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len, 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        int client_fd = co_await op;
        co_return client_fd;
    }

    Task<int> Server::UringEngine::async_read(int fd, std::span<std::byte> buffer)
    {
        UringOperation op;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, fd, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        co_return co_await op;
    }

    Task<int> Server::UringEngine::async_write(int fd, std::span<const std::byte> buffer)
    {
        UringOperation op;
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

        io_uring_prep_send(sqe, fd, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);

        co_return co_await op;
    }

    Task<void> Server::UringEngine::async_read_exact(int fd, std::span<std::byte> buffer)
    {
        size_t total_read = 0;
        while (total_read < buffer.size()) {
            const int bytes = co_await async_read(fd, buffer.subspan(total_read));
            if (bytes == 0) {
                throw std::runtime_error("Connexion fermee par le client");
            }
            total_read += bytes;
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

        uint16_t cmd = 0;
        uint64_t sid = 0;
        std::istringstream iss(header_line);
        std::string token;
        iss >> token;

        // Essayer de parser comme nombre d'abord
        auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), cmd);
        if (ec != std::errc()) {
            // Pas un nombre → chercher dans le mapping des noms de commandes
            auto it = server_.text_command_names_.find(token);
            if (it != server_.text_command_names_.end()) {
                cmd = it->second;
            } else {
                LOG(Logger::LogLevel::WARN, "[Texte] Nom de commande inconnu : %s", token.c_str());
                co_return frame;
            }
        }

        iss >> sid;
        frame.header.command_id = cmd;
        frame.header.session_id = sid;
        frame.header.flags = 0;

        const std::string payload_line = co_await read_text_line(client_fd);
        frame.header.payload_length = static_cast<uint32_t>(payload_line.size());
        frame.payload.resize(frame.header.payload_length);
        if (!payload_line.empty()) {
            std::memcpy(frame.payload.data(), payload_line.data(), frame.header.payload_length);
        }

        co_return frame;
    }

    Task<void> Server::UringEngine::process_command(
        const ClientFrame& frame, IConnection& connection, Session& session) const
    {
        auto endpoint = server_.router_.get(frame.header.command_id);

        if (!endpoint) {
            LOG(Logger::LogLevel::WARN, "[Rejet] Commande inconnue : %u", frame.header.command_id);
            co_return;
        }

        Context ctx(frame.header, frame.payload, session, connection);

        bool is_auth_ok = true;
        if (endpoint->requires_auth) {
            is_auth_ok = co_await server_.authenticator_->authenticate(ctx);
        }

        const bool is_transport_ok = (endpoint->allowed_transport == TransportType::ANY ||
            endpoint->allowed_transport == connection.transport_type());

        if (!is_auth_ok || !is_transport_ok) {
            LOG(Logger::LogLevel::WARN, "[Rejet] Securite (%d) ou Transport (%d) invalide pour CMD %u",
                is_auth_ok, is_transport_ok, frame.header.command_id);
            co_return;
        }

        auto [flags, payload] = co_await endpoint->handler(ctx);

        const FrameHeader res_header {
            frame.header.command_id,
            flags,
            static_cast<uint32_t>(payload.size()),
            session.id()
        };

        co_await connection.send_frame(res_header, payload);
    }

    DetachedTask Server::UringEngine::handle_client(int client_fd)
    {
        ++active_connections_;

        UringTcpConnection connection(client_fd, *this);
        auto current_session_id = static_cast<uint64_t>(-1);

        while (running)
        {
            try {
                ClientFrame frame = co_await read_frame(client_fd);

                if (!server_.session_store_) {
                    throw std::runtime_error("SessionStore non initialise");
                }
                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);

                if (frame.header.session_id != current_session_id) {
                    if (current_session_id != 0) unregister_session(current_session_id);
                    register_session(frame.header.session_id, client_fd);
                    current_session_id = frame.header.session_id;
                }

                co_await process_command(frame, connection, session);

                co_await server_.session_store_->save(session);

            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::WARN, "[Deconnexion/Erreur] Client %d : %s", client_fd, e.what());
                break;
            }
        }
        if (current_session_id != static_cast<uint64_t>(-1)) unregister_session(current_session_id);
        close(client_fd);

        --active_connections_;
    }

    DetachedTask Server::UringEngine::text_handle_client(int client_fd)
    {
        ++active_connections_;

        TextTcpConnection connection(client_fd, *this);
        auto current_session_id = static_cast<uint64_t>(-1);

        while (running)
        {
            try {
                ClientFrame frame = co_await read_text_frame(client_fd);

                if (!server_.session_store_) {
                    throw std::runtime_error("SessionStore non initialise");
                }
                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);

                if (frame.header.session_id != current_session_id) {
                    if (current_session_id != 0) unregister_session(current_session_id);
                    register_session(frame.header.session_id, client_fd);
                    current_session_id = frame.header.session_id;
                }

                co_await process_command(frame, connection, session);

                co_await server_.session_store_->save(session);

            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::WARN, "[Deconnexion/Erreur] Client %d : %s", client_fd, e.what());
                break;
            }
        }
        if (current_session_id != static_cast<uint64_t>(-1)) unregister_session(current_session_id);
        close(client_fd);

        --active_connections_;
    }

    DetachedTask Server::UringEngine::start_accept_loop(int server_fd, ProtocolMode mode)
    {
        while (running) {
            try {
                const int client_fd = co_await async_accept(server_fd);

                if (server_.max_clients_ > 0 && active_connections_ >= server_.max_clients_) {
                    LOG(Logger::LogLevel::WARN, "[Rejet] Limite de clients atteinte (%zu), connexion refusee", server_.max_clients_);
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

    Task<size_t> Server::UringEngine::async_recvmsg(int fd, std::span<std::byte> buffer, struct sockaddr_storage& addr)
    {
        UringOperation op;
        struct iovec iov{buffer.data(), buffer.size()};
        struct msghdr msg{};
        msg.msg_name = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recvmsg(sqe, fd, &msg, 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        co_return static_cast<size_t>(co_await op);
    }

    Task<int> Server::UringEngine::async_sendto(int fd, std::span<const std::byte> buffer, const struct sockaddr_storage& addr)
    {
        UringOperation op;
        struct iovec iov{const_cast<std::byte*>(buffer.data()), buffer.size()};
        struct msghdr msg{};
        msg.msg_name = const_cast<struct sockaddr_storage*>(&addr);
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_sendmsg(sqe, fd, &msg, 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        co_return co_await op;
    }

    DetachedTask Server::UringEngine::start_udp_loop(int udp_fd)
    {
        std::array<std::byte, 65536> buffer{};

        while (running) {
            try {
                struct sockaddr_storage client_addr{};
                const size_t bytes = co_await async_recvmsg(udp_fd, buffer, client_addr);

                if (bytes < sizeof(FrameHeader)) continue;

                FrameHeader header;
                std::memcpy(&header, buffer.data(), sizeof(FrameHeader));

                const uint32_t payload_len = std::min(
                    header.payload_length,
                    static_cast<uint32_t>(bytes - sizeof(FrameHeader)));
                header.payload_length = payload_len;

                std::vector<std::byte> payload(payload_len);
                if (payload_len > 0) {
                    std::memcpy(payload.data(), buffer.data() + sizeof(FrameHeader), payload_len);
                }

                ClientFrame frame{header, std::move(payload)};
                UringUdpConnection connection(udp_fd, *this, client_addr);

                if (!server_.session_store_) continue;
                Session session = co_await server_.session_store_->get_or_create(frame.header.session_id);

                co_await process_command(frame, connection, session);

                co_await server_.session_store_->save(session);

            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::ERROR, "[Erreur UDP] %s", e.what());
            }
        }
    }

    DetachedTask Server::UringEngine::periodic_timer_loop(std::chrono::milliseconds interval, PeriodicTaskHandler handler)
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
            const int _ = co_await op;
            (void)_;

            try {
                co_await handler(server_);
            } catch (const std::exception& e) {
                LOG(Logger::LogLevel::ERROR, "[Timer] Erreur: %s", e.what());
            }
        }
    }

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
