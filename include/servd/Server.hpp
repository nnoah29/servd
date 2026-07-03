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

#include "servd/Protocol.hpp"
#include "servd/Task.hpp"
#include "servd/router/Router.hpp"
#include "servd/interfaces/ISessionStore.hpp"
#include "servd/interfaces/IAuthenticator.hpp"

namespace servd {

    using bytes = std::span<const std::byte>;

    struct DiscoveryConfig {
        uint16_t broadcast_port = 9999;
        uint32_t magic_number = 0x53525644;
        bool respond_to_clients = true;
        std::chrono::seconds active_announce_if_idle{0};
    };

    using PeriodicTaskHandler = std::function<Task<void>(class Server& server)>;

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
            Server& set_session_store(std::shared_ptr<ISessionStore> store);
            Server& set_authenticator(std::shared_ptr<IAuthenticator> authenticator);
            Server& set_encryption(std::array<uint8_t, 32> psk = {});
            Server& set_max_clients(size_t max);
            Server& load_config(const std::string& path);
            Server& add_command_name(const std::string& name, uint16_t command_id);

            Endpoint& add_command(uint16_t command_id, Handler handler);
            void add_periodic_task(std::chrono::milliseconds interval, PeriodicTaskHandler handler);

            Task<void> send_to(uint64_t session_id, uint16_t command_id, bytes payload) const;
            Task<void> broadcast(uint16_t command_id, bytes payload) const;
            Task<void> broadcast_if(uint16_t command_id, bytes payload,
                std::function<bool(const Session&)> predicate) const;

            void init();
            void run();
            void stop();

        private:
            Router router_;
            std::shared_ptr<ISessionStore> session_store_;
            std::shared_ptr<IAuthenticator> authenticator_; // <-- NOUVEAU

            std::vector<std::pair<uint16_t, ProtocolMode>> tcp_ports_;
            std::vector<uint16_t> udp_ports_;
            std::vector<std::pair<std::string, ProtocolMode>> unix_paths_;
            DiscoveryConfig discovery_config_;
            std::vector<PeriodicTaskInfo> periodic_tasks_;
            size_t max_clients_ = 0;
            EncryptionMode encryption_mode_ = EncryptionMode::NONE;
            std::array<uint8_t, 32> encryption_key_{};
            std::unordered_map<std::string, uint16_t> text_command_names_;

            struct UringEngine;
            std::unique_ptr<UringEngine> engine_;
    };
}