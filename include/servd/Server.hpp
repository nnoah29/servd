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
#include <memory>
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <span>
#include <cstdint>
#include <cstddef>
#include <netinet/in.h>

#include "servd/Protocol.hpp"
#include "servd/Task.hpp"
#include "servd/ThreadPool.hpp"
#include "servd/router/Router.hpp"
#include "servd/interfaces/ISessionStore.hpp"
#include "servd/interfaces/IAuthenticator.hpp"
#include "servd/Logger.hpp"

struct sd_bus;

namespace servd {

    using bytes = std::span<const std::byte>;

    using DiscoveryResponder = std::function<void(
        const struct sockaddr_in& client_addr,
        DiscoveryPacket& response
    )>;

    struct DiscoveryConfig {
        uint16_t broadcast_port = 9999;
        uint32_t magic_number = 0x53525644;
        bool respond_to_clients = true;
        std::chrono::seconds active_announce_if_idle{0};
        uint16_t advertised_tcp_port = 0;
        uint16_t advertised_udp_port = 0;
        DiscoveryResponder on_discovery_request{};
    };

    using PeriodicTaskHandler = std::function<Task<void>(class Server& server)>;
    using DisconnectHandler = std::function<void(uint64_t session_id)>;

    struct PeriodicTaskInfo {
        std::chrono::milliseconds interval;
        PeriodicTaskHandler handler;
    };

    class Server {
        public:
            Server();
            ~Server();
            Server(const Server&) = delete;
            Server& operator=(const Server&) = delete;

            Server& enable_tcp(uint16_t port, ProtocolMode mode = ProtocolMode::BINARY);
            Server& enable_udp(uint16_t port);
            Server& enable_unix_socket(const std::string& path, ProtocolMode mode = ProtocolMode::BINARY);
            Server& enable_discovery(const DiscoveryConfig& config);
            Server& disable_discovery();
            Server& set_session_store(std::shared_ptr<ISessionStore> store);
            Server& set_authenticator(std::shared_ptr<IAuthenticator> authenticator);

            Server& enable_session_bus();
            Server& enable_system_bus();

            Server& set_max_clients(size_t max);
            Server& load_config(const std::string& path);
            Server& add_command_name(const std::string& name, uint16_t command_id);

            Endpoint& add_command(uint16_t command_id, Handler handler);
            void add_periodic_task(std::chrono::milliseconds interval, PeriodicTaskHandler handler);
            Server& on_disconnect(DisconnectHandler handler);

            Task<void> send_to(uint64_t session_id, uint16_t command_id, bytes payload) const;
            Task<void> broadcast(uint16_t command_id, bytes payload) const;
            Task<void> broadcast_if(uint16_t command_id, bytes payload,
                std::function<bool(const Session&)> predicate) const;

            ThreadPool& thread_pool() const;
            void enable_thread_pool(size_t thread_count = 4);

            void init();
            void run() const;
            void stop() const;

        private:
            void init_defaults();
            void init_tcp_sockets();
            void init_unix_sockets();
            void init_periodic_tasks() const;
            void init_dbus() const;
            void init_udp_sockets() const;
            void init_discovery();
            Router router_;
            std::shared_ptr<ISessionStore> session_store_;
            std::shared_ptr<IAuthenticator> authenticator_;

            std::vector<std::pair<uint16_t, ProtocolMode>> tcp_ports_;
            std::vector<uint16_t> udp_ports_;
            std::vector<std::pair<std::string, ProtocolMode>> unix_paths_;
            DiscoveryConfig discovery_config_;
            bool discovery_enabled_ = false;
            std::vector<PeriodicTaskInfo> periodic_tasks_;
            DisconnectHandler disconnect_handler_;
            size_t max_clients_ = 0;
            int discovery_fd_ = -1;
            std::unordered_map<std::string, uint16_t> text_command_names_;

            sd_bus* session_bus_ = nullptr;
            sd_bus* system_bus_ = nullptr;

            std::unique_ptr<ThreadPool> thread_pool_;

            struct UringEngine;
            std::unique_ptr<UringEngine> engine_;
    };
}