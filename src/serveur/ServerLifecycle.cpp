#include "detail/Engine.hpp"
#include <servd/store/InMemorySessionStore.hpp>
#include <servd/auth/DefaultAuthenticator.hpp>
#include <Logger.hpp>
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
            LOG(Logger::LogLevel::INFO, "[Serveur] Aucun SessionStore defini, utilisation de InMemorySessionStore par defaut.");
            session_store_ = std::make_shared<InMemorySessionStore>();
        }

        if (!authenticator_) {
            LOG(Logger::LogLevel::INFO, "[Serveur] Aucun Authenticator defini, utilisation de DefaultAuthenticator par defaut.");
            authenticator_ = std::make_shared<DefaultAuthenticator>();
        }

        engine_->running = true;

        for (const auto& [port, mode] : tcp_ports_) {
            const int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) throw std::runtime_error("Erreur creation socket TCP");

            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);

            if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
                throw std::runtime_error("Erreur bind() sur le port TCP " + std::to_string(port));

            if (listen(fd, SOMAXCONN) < 0)
                throw std::runtime_error("Erreur listen() TCP");

            LOG(Logger::LogLevel::INFO, "[Serveur] Ecoute TCP sur le port %u", port);
            engine_->start_accept_loop(fd, mode);
        }

        for (const auto& [path, mode] : unix_paths_) {
            const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) throw std::runtime_error("Erreur creation socket UNIX");

            unlink(path.c_str());

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

            if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
                throw std::runtime_error("Erreur bind() sur socket UNIX " + path);

            if (listen(fd, SOMAXCONN) < 0)
                throw std::runtime_error("Erreur listen() UNIX");

            LOG(Logger::LogLevel::INFO, "[Serveur] Ecoute UNIX sur %s", path.c_str());
            engine_->start_accept_loop(fd, mode);
        }

        for (const auto& task : periodic_tasks_) {
            engine_->periodic_timer_loop(task.interval, task.handler);
        }

        if (session_bus_) {
            LOG(Logger::LogLevel::INFO, "[Serveur] Bus session D-Bus actif");
            engine_->bus_monitor_loop(session_bus_);
        }

        if (system_bus_) {
            LOG(Logger::LogLevel::INFO, "[Serveur] Bus systeme D-Bus actif");
            engine_->bus_monitor_loop(system_bus_);
        }

        for (const uint16_t port : udp_ports_) {
            const int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0) throw std::runtime_error("Erreur creation socket UDP");

            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);

            if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
                throw std::runtime_error("Erreur bind() UDP sur le port " + std::to_string(port));

            LOG(Logger::LogLevel::INFO, "[Serveur] Ecoute UDP sur le port %u", port);
            engine_->start_udp_loop(fd);
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

    ThreadPool& Server::thread_pool()
    {
        if (!thread_pool_)
            throw std::runtime_error("ThreadPool not enabled. Call enable_thread_pool() before init().");
        return *thread_pool_;
    }

    void Server::enable_thread_pool(size_t thread_count)
    {
        thread_pool_ = std::make_unique<ThreadPool>(thread_count,
            [this](std::coroutine_handle<> h) {
                engine_->post_coroutine(h);
            });
    }

}
