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
#include <vector>
#include <cstddef>
#include <array>

namespace servd {

    template<typename T> class Task;

    enum class TransportType : uint8_t {
        ANY = 0, TCP = 1,
        UDP = 2, UNIX = 3
    };

    enum class ProtocolMode : uint8_t {
        BINARY = 0,
        TEXT   = 1
    };

    enum class EncryptionMode : uint8_t {
        NONE = 0,
        RSA  = 1
    };

    #pragma pack(push, 1)
    struct FrameHeader {
        uint16_t command_id;
        uint16_t flags;
        uint32_t payload_length;
        uint64_t session_id;
    };
    #pragma pack(pop)

    struct ResponseFrame {
        uint16_t flags = 0;
        std::vector<std::byte> payload;
    };

    enum class DiscoveryAction : uint8_t {
        CLIENT_LOOKING_FOR_SERVER = 0x01,
        SERVER_ANNOUNCING         = 0x02
    };

    #pragma pack(push, 1)
    struct DiscoveryPacket {
        uint32_t magic_number;
        DiscoveryAction action;
        int32_t tcp_port;
        int32_t udp_port;
    };
    #pragma pack(pop)
}
