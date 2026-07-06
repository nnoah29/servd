# API : Protocole

## Header

```cpp
#include <servd/Protocol.hpp>
```

## Énumérations

### `TransportType`

```cpp
enum class TransportType : uint8_t {
    ANY  = 0,
    TCP  = 1,
    UDP  = 2,
    UNIX = 3
};
```

### `ProtocolMode`

```cpp
enum class ProtocolMode : uint8_t {
    BINARY = 0,  // 16-byte FrameHeader + payload (recommandé)
    TEXT   = 1   // Header ligne + payload ligne (10× plus lent)
};
```

### `DiscoveryAction`

```cpp
enum class DiscoveryAction : uint8_t {
    CLIENT_LOOKING_FOR_SERVER = 0x01,
    SERVER_ANNOUNCING         = 0x02
};
```

## Structures

### `FrameHeader` (16 bytes, packed)

```cpp
struct FrameHeader {
    uint16_t command_id;
    uint16_t flags;
    uint32_t payload_length;
    uint64_t session_id;
} __attribute__((packed));
```

### `ResponseFrame`

```cpp
struct ResponseFrame {
    uint16_t flags;
    std::vector<std::byte> payload;
};
```

### `DiscoveryPacket`

```cpp
struct DiscoveryPacket {
    uint32_t magic_number;
    DiscoveryAction action;
    int32_t tcp_port;
    int32_t udp_port;
} __attribute__((packed));
```

## Constantes

```cpp
constexpr uint16_t CMD_KEY_EXCHANGE      = 0x00F0;
constexpr uint16_t CMD_ENCRYPTED_MESSAGE = 0x00F1;
```

## Exemple : construction d'un FrameHeader

```cpp
FrameHeader header;
header.command_id = 0x01;
header.flags = 0;
header.payload_length = static_cast<uint32_t>(payload.size());
header.session_id = session.id();
```

## Exemple : parsing côté serveur

```cpp
// Dans un handler
const FrameHeader& hdr = ctx.header();
uint16_t cmd = hdr.command_id;
uint64_t sid = hdr.session_id;
uint32_t len = hdr.payload_length;
```
