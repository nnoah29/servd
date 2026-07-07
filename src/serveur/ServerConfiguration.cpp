#include "detail/Engine.hpp"
#include <systemd/sd-bus.h>

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

    Server& Server::enable_session_bus() {
        sd_bus_default_user(&session_bus_);
        return *this;
    }

    Server& Server::enable_system_bus() {
        sd_bus_default_system(&system_bus_);
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

    Server& Server::set_max_clients(size_t max) {
        max_clients_ = max;
        return *this;
    }

    void Server::add_periodic_task(std::chrono::milliseconds interval, PeriodicTaskHandler handler) {
        periodic_tasks_.push_back({interval, std::move(handler)});
    }

}
