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
#include <servd/store/InMemorySessionStore.hpp>
#include <servd/auth/DefaultAuthenticator.hpp>
#include <Logger.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace servd
{

    Server::Server() : engine_(std::make_unique<UringEngine>(*this)) {}
    Server::~Server() = default;

    Server& Server::enable_tcp(uint16_t port, ProtocolMode mode) {
        tcp_ports_.emplace_back(port, mode);
        return *this;
    }

    Server& Server::enable_udp(uint16_t port) {
        udp_ports_.push_back(port);
        return *this;
    }

    Server& Server::enable_unix_socket(const std::string& path, ProtocolMode mode) {
        unix_paths_.emplace_back(path, mode);
        return *this;
    }

    Server& Server::enable_discovery(const DiscoveryConfig& config) {
        discovery_config_ = config;
        return *this;
    }

    Server& Server::set_session_store(std::shared_ptr<ISessionStore> store) {
        session_store_ = std::move(store);
        return *this;
    }

    Server& Server::set_authenticator(std::shared_ptr<IAuthenticator> authenticator) {
        authenticator_ = std::move(authenticator);
        return *this;
    }

    Server& Server::set_encryption(std::array<uint8_t, 32> psk) {
        encryption_key_ = psk;
        encryption_mode_ = EncryptionMode::PSK;
        return *this;
    }

    Server& Server::set_max_clients(size_t max) {
        max_clients_ = max;
        return *this;
    }

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

    void Server::run() {
        engine_->run();
    }

    void Server::stop() {
        engine_->running = false;
    }

    static Logger::LogLevel parse_log_level(const std::string& s) {
        if (s == "DEBUG") return Logger::LogLevel::DEBUG;
        if (s == "INFO")  return Logger::LogLevel::INFO;
        if (s == "WARN")  return Logger::LogLevel::WARN;
        if (s == "ERROR") return Logger::LogLevel::ERROR;
        return Logger::LogLevel::INFO;
    }

    static std::string trim(std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
        return s;
    }

    Server& Server::load_config(const std::string& path) {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Impossible d'ouvrir le fichier de config: " + path);

        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            const std::string key = trim(line.substr(0, eq));
            const std::string value = trim(line.substr(eq + 1));

            if (key == "tcp")       enable_tcp(static_cast<uint16_t>(std::stoi(value)));
            else if (key == "udp")  enable_udp(static_cast<uint16_t>(std::stoi(value)));
            else if (key == "unix") enable_unix_socket(value);
            else if (key == "max_clients") set_max_clients(static_cast<size_t>(std::stoul(value)));
            else if (key == "log_level") Logger::setLevel(parse_log_level(value));
            else if (key == "log_file")  Logger::setLogFile(value);
        }
        return *this;
    }

    void Server::add_periodic_task(std::chrono::milliseconds interval, PeriodicTaskHandler handler) {
        periodic_tasks_.push_back({interval, std::move(handler)});
    }

    Task<void> Server::send_to(uint64_t session_id, uint16_t command_id, bytes payload) const
    {
        return engine_->send_to_session(session_id, command_id, payload);
    }

    Task<void> Server::broadcast(uint16_t command_id, bytes payload) const
    {
        return engine_->do_broadcast(command_id, payload);
    }

    Task<void> Server::broadcast_if(uint16_t command_id, bytes payload, std::function<bool(const Session&)> predicate) const {
        return engine_->do_broadcast_if(command_id, payload, std::move(predicate));
    }

    Server& Server::add_command_name(const std::string& name, uint16_t command_id) {
        text_command_names_[name] = command_id;
        return *this;
    }

    Endpoint& Server::add_command(uint16_t command_id, Handler handler){
        return router_.add(command_id, std::move(handler));
    }

}
