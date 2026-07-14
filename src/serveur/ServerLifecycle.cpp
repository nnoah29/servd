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
        if (!session_store_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] No SessionStore defined, using InMemorySessionStore by default.");
            session_store_ = std::make_shared<InMemorySessionStore>();
        }

        if (!authenticator_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] No Authenticator defined, using DefaultAuthenticator by default.");
            authenticator_ = std::make_shared<DefaultAuthenticator>();
        }

        engine_->running = true;

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

        for (const auto& task : periodic_tasks_) {
            engine_->periodic_timer_loop(task.interval, task.handler);
        }

        if (session_bus_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] D-Bus session bus active");
            engine_->bus_monitor_loop(session_bus_);
        }

        if (system_bus_) {
            SERVD_LOG(Logger::LogLevel::INFO, "[Server] D-Bus system bus active");
            engine_->bus_monitor_loop(system_bus_);
        }

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

        SERVD_LOG(Logger::LogLevel::INFO, "[Server] Initialization complete");
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
