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
#include <string>
#include <span>
#include <cstddef>
#include "servd/Protocol.hpp"

namespace servd {

class IConnection {
    public:
        virtual ~IConnection() = default;
        virtual TransportType transport_type() const = 0;
        virtual Task<void> send_frame(const FrameHeader& header, std::span<const std::byte> payload) = 0;
        virtual std::string get_remote_address() const = 0;
    };
}
