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
#include <functional>
#include "servd/Protocol.hpp"
#include "servd/Context.hpp"

namespace servd {

    class Context;
    using Handler = std::function<Task<ResponseFrame>(Context&)>;

    class Endpoint {
        public:
            Handler handler = nullptr;
            bool requires_auth = false;
            TransportType allowed_transport = TransportType::ANY;

            Endpoint& require_auth() { requires_auth = true; return *this; }
            Endpoint& tcp_only()     { allowed_transport = TransportType::TCP; return *this; }
            Endpoint& udp_only()     { allowed_transport = TransportType::UDP; return *this; }

            [[nodiscard]] bool is_valid() const { return handler != nullptr; }
        };
}
