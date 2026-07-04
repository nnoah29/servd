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

#pragma once

#include <liburing.h>
#include <span>
#include <vector>
#include <string>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <coroutine>
#include <system_error>
#include <stdexcept>

#include <sys/socket.h>
#include <servd/Server.hpp>
#include <servd/interfaces/IConnection.hpp>
#include <servd/crypto/AesGcm.hpp>
#include <servd/crypto/X25519.hpp>
#include <cerrno>

namespace servd
{

    struct ClientFrame {
        FrameHeader header{};
        std::vector<std::byte> payload;
    };

    struct UringOperation
    {
        std::coroutine_handle<> coroutine;
        int cqe_res{};

        static bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> continuation) noexcept { coroutine = continuation; }
        [[nodiscard]] int await_resume() const {
            if (cqe_res < 0 &&  cqe_res != -ETIME)
                throw std::system_error(-cqe_res, std::system_category(), "io_uring operation failed");
            return cqe_res;
        }
    };

    struct Server::UringEngine
    {
        Server& server_;
        struct io_uring ring{};
        bool running = false;

        explicit UringEngine(Server& s);
        ~UringEngine();

        void run();

        Task<int> async_accept(int server_fd);
        Task<int> async_read(int fd, std::span<std::byte> buffer);
        Task<int> async_write(int fd, std::span<const std::byte> buffer);
        Task<void> async_read_exact(int fd, std::span<std::byte> buffer);
        class UringTcpConnection final : public IConnection {
            public:
                UringTcpConnection(int fd, UringEngine& engine);
                [[nodiscard]] TransportType transport_type() const override { return TransportType::TCP; }
                Task<void> send_frame(const FrameHeader& header, std::span<const std::byte> payload) override;
                [[nodiscard]] std::string get_remote_address() const override { return "unknown (TODO)"; }
            private:
                int fd_;
                UringEngine& engine_;
        };

        class TextTcpConnection final : public IConnection {
            public:
                TextTcpConnection(int fd, UringEngine& engine);
                [[nodiscard]] TransportType transport_type() const override { return TransportType::TCP; }
                Task<void> send_frame(const FrameHeader& header, std::span<const std::byte> payload) override;
                [[nodiscard]] std::string get_remote_address() const override { return "unknown (TODO)"; }
            private:
                int fd_;
                UringEngine& engine_;
        };

        Task<ClientFrame> read_frame(int client_fd);
        Task<std::string> read_text_line(int client_fd);
        Task<ClientFrame> read_text_frame(int client_fd);
        Task<size_t> async_recvmsg(int fd, std::span<std::byte> buffer, struct sockaddr_storage& addr);
        Task<int> async_sendto(int fd, std::span<const std::byte> buffer, const struct sockaddr_storage& addr);
        Task<void> process_command(const ClientFrame& frame, IConnection& connection, Session& session) const;
        static Task<void> handle_key_exchange(const ClientFrame& frame, IConnection& connection, Session& session);
        Task<void> handle_encrypted_message(const ClientFrame& frame, IConnection& connection, Session& session) const;
        Task<void> handle_normal_command(const ClientFrame& frame, IConnection& connection, Session& session) const;
        DetachedTask handle_client(int client_fd);
        DetachedTask text_handle_client(int client_fd);
        DetachedTask start_accept_loop(int server_fd, ProtocolMode mode = ProtocolMode::BINARY);
        DetachedTask start_udp_loop(int udp_fd);
        DetachedTask periodic_timer_loop(std::chrono::milliseconds interval, PeriodicTaskHandler handler);

        void register_session(uint64_t session_id, int fd);
        void unregister_session(uint64_t session_id);
        Task<void> send_to_session(uint64_t session_id, uint16_t command_id, bytes payload);
        Task<void> do_broadcast(uint16_t command_id, bytes payload);
        Task<void> do_broadcast_if(uint16_t command_id, bytes payload,
            std::function<bool(const Session&)> predicate);

        std::unordered_map<uint64_t, int> sessions_;
        size_t active_connections_ = 0;

        class UringUdpConnection final : public IConnection {
            public:
                UringUdpConnection(int fd, UringEngine& engine, const struct sockaddr_storage& addr);
                [[nodiscard]] TransportType transport_type() const override { return TransportType::UDP; }
                Task<void> send_frame(const FrameHeader& header, std::span<const std::byte> payload) override;
                [[nodiscard]] std::string get_remote_address() const override;
            private:
                int fd_;
                UringEngine& engine_;
                struct sockaddr_storage client_addr_;
        };
    };

}
