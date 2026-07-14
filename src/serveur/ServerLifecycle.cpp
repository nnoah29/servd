#include "detail/Engine.hpp"
#include <servd/store/InMemorySessionStore.hpp>
#include <servd/auth/DefaultAuthenticator.hpp>
#include <servd/Logger.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

namespace servd
{

    void Server::init()
    {
        init_defaults();
        engine_->running = true;
        init_tcp_sockets();
        init_unix_sockets();
        init_periodic_tasks();
        init_dbus();
        init_udp_sockets();
        init_discovery();
        SERVD_LOG(Logger::LogLevel::INFO, "[Server] Initialization complete");
    }

    void Server::init_defaults()
    {
        if (!session_store_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] No SessionStore defined, using InMemorySessionStore by default.");
            session_store_ = std::make_shared<InMemorySessionStore>();
        }

        if (!authenticator_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] No Authenticator defined, using DefaultAuthenticator by default.");
            authenticator_ = std::make_shared<DefaultAuthenticator>();
        }
    }

    void Server::init_tcp_sockets()
    {
        for (const auto& [port, mode] : tcp_ports_) {
            const int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) throw std::runtime_error("Failed to create TCP socket");

            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);

            if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
                throw std::runtime_error("bind() failed on TCP port " + std::to_string(port));

            if (listen(fd, SOMAXCONN) < 0)
                throw std::runtime_error("listen() failed on TCP socket");

            SERVD_LOG(Logger::LogLevel::INFO, "[Server] Listening on TCP port %u", port);
            engine_->start_accept_loop(fd, mode);
        }
    }

    void Server::init_unix_sockets()
    {
        for (const auto& [path, mode] : unix_paths_) {
            const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) throw std::runtime_error("Failed to create UNIX socket");

            unlink(path.c_str());

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

            if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
                throw std::runtime_error("bind() failed on UNIX socket " + path);

            if (listen(fd, SOMAXCONN) < 0)
                throw std::runtime_error("listen() failed on UNIX socket");

            SERVD_LOG(Logger::LogLevel::INFO, "[Server] Listening on UNIX socket %s", path.c_str());
            engine_->start_accept_loop(fd, mode);
        }
    }

    void Server::init_periodic_tasks() const
    {
        for (const auto& [interval, handler] : periodic_tasks_) {
            engine_->periodic_timer_loop(interval, handler);
        }
    }

    void Server::init_dbus() const
    {
        if (session_bus_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] D-Bus session bus active");
            engine_->bus_monitor_loop(session_bus_);
        }

        if (system_bus_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] D-Bus system bus active");
            engine_->bus_monitor_loop(system_bus_);
        }
    }

    void Server::init_udp_sockets() const
    {
        for (const uint16_t port : udp_ports_) {
            const int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0) throw std::runtime_error("Failed to create UDP socket");

            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);

            if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
                throw std::runtime_error("bind() failed on UDP port " + std::to_string(port));

            SERVD_LOG(Logger::LogLevel::INFO, "[Server] Listening on UDP port %u", port);
            engine_->start_udp_loop(fd);
        }
    }

    void Server::init_discovery()
    {
        if (!discovery_enabled_ || discovery_config_.broadcast_port == 0)
            return;

        const int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) throw std::runtime_error("Failed to create discovery UDP socket");

        constexpr int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(discovery_config_.broadcast_port);

        if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed on discovery port " + std::to_string(discovery_config_.broadcast_port));

        SERVD_LOG(Logger::LogLevel::INFO, "[Discovery] Listening on port %u", discovery_config_.broadcast_port);
        discovery_fd_ = fd;
        engine_->start_discovery_loop(fd);

        if (discovery_config_.active_announce_if_idle.count() > 0) {
            const auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                discovery_config_.active_announce_if_idle);
            engine_->periodic_timer_loop(interval,
                [this](Server&) -> Task<void> {
                    if (engine_->active_connections_ > 0 || discovery_fd_ < 0 || !discovery_enabled_)
                        co_return;

                    const auto& cfg = discovery_config_;
                    DiscoveryPacket resp{};
                    resp.magic_number = cfg.magic_number;
                    resp.action = DiscoveryAction::SERVER_ANNOUNCING;
                    resp.tcp_port = cfg.advertised_tcp_port
                        ? cfg.advertised_tcp_port
                        : (tcp_ports_.empty() ? 0 : tcp_ports_[0].first);
                    resp.udp_port = cfg.advertised_udp_port
                        ? cfg.advertised_udp_port
                        : (udp_ports_.empty() ? 0 : udp_ports_[0]);

                    struct sockaddr_in broadcast_addr{};
                    broadcast_addr.sin_family = AF_INET;
                    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
                    broadcast_addr.sin_port = htons(cfg.broadcast_port);

                    struct sockaddr_storage sockaddr_storage{};
                    std::memcpy(&sockaddr_storage, &broadcast_addr, sizeof(broadcast_addr));

                    co_await engine_->async_sendto(discovery_fd_,
                        {reinterpret_cast<std::byte*>(&resp), sizeof(resp)}, sockaddr_storage);
                });
        }
    }

    void Server::run() const
    {
        engine_->run();
    }

    void Server::stop() const
    {
        engine_->running = false;
    }

    ThreadPool& Server::thread_pool() const
    {
        if (!thread_pool_)
            throw std::runtime_error("ThreadPool not enabled. Call enable_thread_pool() before init().");
        return *thread_pool_;
    }

    Server& Server::on_disconnect(DisconnectHandler handler)
    {
        disconnect_handler_ = std::move(handler);
        return *this;
    }

    void Server::enable_thread_pool(size_t thread_count)
    {
        thread_pool_ = std::make_unique<ThreadPool>(thread_count,
            [this](std::coroutine_handle<> h) {
                engine_->post_coroutine(h);
            });
    }

}
