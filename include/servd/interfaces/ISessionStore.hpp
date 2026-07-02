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
#include <cstdint>
#include "servd/Protocol.hpp"
#include "Session.hpp"

namespace servd {

    class ISessionStore {
        public:
            virtual ~ISessionStore() = default;
            virtual Task<Session> get_or_create(uint64_t session_id) = 0;
            virtual Task<void> save(const Session& session) = 0;
    };
}
