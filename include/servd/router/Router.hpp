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
#include <array>
#include <cstdint>
#include "Endpoint.hpp"

namespace servd {

    class Router {
        public:
            Endpoint& add(uint16_t command_id, const Handler& handler) {
                routes_[command_id].handler = handler;
                routes_[command_id].requires_auth = false;
                routes_[command_id].allowed_transport = TransportType::ANY;
                return routes_[command_id];
            }
            const Endpoint* get(uint16_t command_id) const {
                if (!routes_[command_id].is_valid()) return nullptr;
                return &routes_[command_id];
            }
        private:
            std::array<Endpoint, 65536> routes_;
    };

}
