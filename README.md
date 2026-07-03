# servd — M2M Asynchronous Server Framework

**servd** is a high-performance, single-threaded asynchronous server framework for **Machine-to-Machine** (M2M) communication, IoT, and distributed systems. Written in C++20, it leverages **Linux `io_uring`** and **C++20 Coroutines** to deliver minimal latency and maximal throughput — all without a single mutex in the hot path.

---

## Features

| Feature | Description |
|---------|-------------|
| **TCP / UDP / Unix Sockets** | Unified transport on a single `io_uring` loop |
| **C++20 Coroutines** | Async I/O without callback hell |
| **Zero-Copy Binary Protocol** | 16-byte frame header, raw payload as `std::span` — no parsing overhead |
| **O(1) Routing** | `command_id` → direct array index |
| **Injectable Session Store** | Default `InMemorySessionStore`, bring your own via `ISessionStore` |
| **Injectable Authentication** | Default checks `session.is_authenticated()`, bring your own strategy via `IAuthenticator` |
| **Per-Request Property Bag** | `ctx.set<T>()` / `ctx.get<T>()` via `std::any` — pass data from authenticator to handler |
| **Push Events** | Server can push unsolicited frames to a client mid-request |
| **Broadcast** | `broadcast()` / `broadcast_if(predicate)` to all connected clients |
| **Periodic Timers** | Async timers via `IORING_OP_TIMEOUT` — no extra threads |
| **Network Discovery** | UDP broadcast-based zero-config (optional) |
| **Config File** | `.env`-style configuration |

---

## Quick Start

```cpp
#include <servd/Server.hpp>
#include <cstring>

enum Commands : uint16_t {
    CMD_PING  = 0x01,
    CMD_LOGIN = 0x02,
};

int main() {
    servd::Server app;

    // 1. Configure transports
    app.enable_tcp(8080)
       .enable_udp(8081)
       .enable_unix_socket("/tmp/servd.sock");

    // 2. Define routes
    app.add_command(CMD_PING, [](servd::Context& ctx) -> Task<servd::ResponseFrame> {
        std::string msg = "PONG";
        std::vector<std::byte> payload(msg.size());
        std::memcpy(payload.data(), msg.data(), msg.size());
        co_return servd::ResponseFrame{0, payload};
    });

    app.add_command(CMD_LOGIN, [](servd::Context& ctx) -> Task<servd::ResponseFrame> {
        // Parse ctx.payload() — your own protocol
        ctx.session().set_authenticated(true, "user_42");
        co_return servd::ResponseFrame{0, {}};
    }).require_auth().tcp_only();

    // 3. Periodic broadcast to authenticated clients
    app.add_periodic_task(std::chrono::seconds(10), [](servd::Server& s) -> Task<void> {
        std::string alert = "System OK";
        std::vector<std::byte> payload(alert.size());
        std::memcpy(payload.data(), alert.data(), alert.size());
        co_await s.broadcast_if(0x99, payload, [](const servd::Session& s) {
            return s.is_authenticated();
        });
    });

    // 4. Start
    app.init();
    app.run();
}
```

### Build & Run

```bash
cmake -B build && cmake --build build
sudo ./build/servd          # requires CAP_NET_ADMIN for io_uring + discovery
```

**Dependencies:** Linux ≥ 5.1, `liburing`, Botan ≥ 3.0 (`apt install libbotan-3-dev`), C++20 compiler (GCC ≥ 11 / Clang ≥ 14).

---

## Integration Into Your Project

### Option A — CPM (recommended)

Add to your `CMakeLists.txt`:

```cmake
include(cmake/CPM.cmake)  # or download from https://github.com/cpm-cmake/CPM.cmake

CPMAddPackage("gh:nnoah29/servd@0.1.0")

target_link_libraries(my_app PRIVATE servd::servd_core)
```

### Option B — find_package (after install)

```bash
# Build and install servd once
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Then in your project:

```cmake
find_package(servd REQUIRED)
target_link_libraries(my_app PRIVATE servd::servd_core)
```

### Option C — git submodule / add_subdirectory

```bash
git submodule add https://github.com/nnoah29/servd extern/servd
```

```cmake
add_subdirectory(extern/servd)
target_link_libraries(my_app PRIVATE servd::servd_core)
```

---

## The Binary Protocol

Every frame exchanged over the wire has this 16-byte header:

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0 | `uint16_t` | `command_id` | Route identifier (O(1) lookup) |
| 2 | `uint16_t` | `flags` | Metadata (reserved for future use) |
| 4 | `uint32_t` | `payload_length` | Size of the payload in bytes |
| 8 | `uint64_t` | `session_id` | Session identifier (survives reconnections) |

The payload follows immediately as raw bytes. There is no built-in serialization — you define your own protocol on top of this frame. Examples: raw C structs, Protocol Buffers, FlatBuffers, CBOR, JSON, etc.

---

## Text Protocol Mode

In addition to the binary protocol, each TCP/Unix port can be configured in **text mode** for human-readable debugging, scripting, or integration with tools like `netcat`.

```cpp
app.enable_tcp(8081, ProtocolMode::TEXT);
```

**Under the hood, the engine is unchanged.** The text mode is a **wire format adapter** that translates between human-readable text and the internal binary `ClientFrame`. The router, authenticator, session store, and all handlers work identically.

### Wire Format

```
<command_id> <session_id>\n
<payload>\n
```

Each frame is exactly two lines terminated by `\n`:

| Line | Content | Example |
|------|---------|---------|
| 1 | `command_id` (number or name) + space + `session_id` (number) | `2 42` |
| 2 | Payload text (anything up to `\n`) | `alice:my_secret_key` |

**Response format** (server → client):

```
<command_id> <flags> <session_id>\n
<payload>\n
```

### Command Names

The `command_id` can be either a number or a string name registered with `add_command_name()`:

```cpp
app.add_command(CMD_LOGIN, handler).require_auth();
app.add_command_name("login", CMD_LOGIN);  // for text mode
```

Both of these work with `netcat`:

```bash
# By number
printf "2 0\nalice:key\n" | nc localhost 8081

# By name (requires add_command_name)
printf "login 0\nalice:key\n" | nc localhost 8081
```

### Important Caveats

| Constraint | Reason |
|-----------|--------|
| `command_id` and `session_id` are **numbers** internally | The binary `FrameHeader` uses `uint16_t` / `uint64_t` |
| Payload cannot contain `\n` | `\n` terminates the payload line |
| Payload is text-only (no binary data) | No base64 or escaping built-in |
| ~10× slower than binary mode | `read_text_line` reads one byte at a time |
| UDP does not support text mode | Only TCP/Unix stream connections |

### Example Handler

```cpp
app.add_command(CMD_LOGIN, [](Context& ctx) -> Task<ResponseFrame> {
    // ctx.payload() contains the text sent by the client
    std::string_view data{
        reinterpret_cast<const char*>(ctx.payload().data()),
        ctx.payload().size()
    };

    if (data.starts_with("alice")) {
        ctx.session().set_authenticated(true, "alice");
    }

    std::string resp = "OK";
    std::vector<std::byte> payload(resp.size());
    std::memcpy(payload.data(), resp.data(), resp.size());
    co_return ResponseFrame{0, payload};
});
```

The handler code is **identical** to the binary mode. Only the `enable_tcp` line changes.

---

---

## Encryption & Security (AES-256-GCM + X25519)

servd provides built-in **end-to-end encryption** per session using **X25519** key agreement and **AES-256-GCM** authenticated encryption — all powered by **Botan 3** (no OpenSSL).

### How It Works

Two reserved command IDs implement the security layer transparently:

| Command ID | Name | Purpose |
|-----------|------|---------|
| `0x00F0` | `CMD_KEY_EXCHANGE` | X25519 handshake |
| `0x00F1` | `CMD_ENCRYPTED_MESSAGE` | AES-256-GCM encrypted application frame |

### Handshake Protocol (X25519)

The client initiates the handshake by sending `CMD_KEY_EXCHANGE` with its raw 32-byte X25519 public key:

```
Client → Server:  Frame{CMD_KEY_EXCHANGE, session_id, payload = client_public_key[32]}
```

The server:
1. Generates an ephemeral X25519 key pair
2. Computes the shared secret: `X25519(server_private, client_public)`
3. Stores the 32-byte shared secret as the AES-256-GCM key in the session store (`session.set_aes_key()`)
4. Replies with its own 32-byte X25519 public key:

```
Server → Client:  Frame{CMD_KEY_EXCHANGE, session_id, payload = server_public_key[32]}
```

The client derives the same shared secret locally. From this point, both sides share a 256-bit key that **never transited the network**.

### Encrypted Messages (AES-256-GCM)

Once the handshake is complete, the client wraps every application command inside `CMD_ENCRYPTED_MESSAGE`:

```
Outer frame:
  command_id = 0x00F1 (CMD_ENCRYPTED_MESSAGE)
  payload    = [12-byte IV][16-byte GCM Tag][AES-256-GCM ciphertext]
                  ↑ random nonce     ↑ integrity proof    ↑ inner frame
```

The **inner frame** (plaintext after decryption) is a mini binary structure:

| Size | Field | Description |
|------|-------|-------------|
| 2 bytes | `inner_command_id` | The real command (e.g. `CMD_PING`, `CMD_LOGIN`) |
| variable | `inner_payload` | The real payload |

### Server-Side Processing Pipeline

When the server receives a `CMD_ENCRYPTED_MESSAGE`:

```
1. Extract IV (12B) + tag (16B) + ciphertext from payload
2. Look up AES key from session (session.aes_key())
3. AES-256-GCM decrypt → inner_command_id + inner_payload
4. Route inner_command_id to the registered handler
5. Encrypt the handler's response the same way
6. Send back CMD_ENCRYPTED_MESSAGE with [IV + tag + ciphertext]
```

The tag is verified automatically by Botan — if authentication fails, the connection is dropped immediately.

### Client Implementation Guide

Any client capable of X25519 + AES-256-GCM can talk to a servd server. Example in Python:

```python
import os, struct
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# ── Handshake ──────────────────────────────────────────────
client_sk = X25519PrivateKey.generate()
client_pk = client_sk.public_key().public_bytes_raw()  # 32 bytes

# Send CMD_KEY_EXCHANGE
header = struct.pack('<HHIQ', 0x00F0, 0, 32, session_id)
socket.send(header + client_pk)

# Receive server reply
resp_header = socket.read(16)
resp_payload = socket.read(32)
server_pk = resp_payload  # 32 bytes

# Derive shared key
shared_key = client_sk.exchange(
    X25519PublicKey.from_public_bytes(server_pk)
)  # 32 bytes for AES-256

aes = AESGCM(shared_key)

# ── Send encrypted command ────────────────────────────────
inner = struct.pack('<H', CMD_PING) + b'PONG'
iv = os.urandom(12)
ct = aes.encrypt(iv, inner, None)
outer_payload = iv + ct  # [12B IV][16B tag][ciphertext]

header = struct.pack('<HHIQ', 0x00F1, 0, len(outer_payload), session_id)
socket.send(header + outer_payload)

# ── Receive encrypted response ────────────────────────────
resp_header = socket.read(16)
resp_payload = socket.read(struct.unpack('<I', resp_header[4:8])[0])
resp_iv = resp_payload[:12]
resp_ct = resp_payload[12:]  # includes tag
inner_resp = aes.decrypt(resp_iv, resp_ct, None)  # [inner_cmd_id][payload]
```

### Session API (additions for encryption)

```cpp
bool has_aes_key() const;                          // true after handshake
void set_aes_key(const std::array<uint8_t, 32>&);  // store AES session key
const std::array<uint8_t, 32>& aes_key() const;    // retrieve AES session key
```

### Security Notes

| Concern | Detail |
|---------|--------|
| **Key freshness** | New X25529 key pair on every handshake (ephemeral-ephemeral) |
| **Nonce uniqueness** | Server generates a cryptographic random IV per message (12 bytes via `getrandom`) |
| **Integrity** | AES-GCM tag rejects tampered or replayed ciphertexts |
| **Forward secrecy** | Ephemeral X25519 keys are not persisted — past traffic cannot be decrypted after session end |
| **No cleartext commands** | Every frame with `CMD_ENCRYPTED_MESSAGE` is fully authenticated and encrypted |

---

## API Reference

### Server Configuration

```cpp
Server& enable_tcp(uint16_t port);
Server& enable_udp(uint16_t port);
Server& enable_unix_socket(const std::string& path);
Server& enable_discovery(const DiscoveryConfig& config);
Server& set_session_store(std::shared_ptr<ISessionStore> store);
Server& set_authenticator(std::shared_ptr<IAuthenticator> authenticator);
Server& load_config(const std::string& path);
```

### Routing

```cpp
Endpoint& add_command(uint16_t command_id, Handler handler);

// On the returned Endpoint:
Endpoint& require_auth();   // Reject if authentication fails
Endpoint& tcp_only();       // Reject if not received over TCP
Endpoint& udp_only();       // Reject if not received over UDP
```

### Handler Signature

```cpp
using Handler = std::function<Task<ResponseFrame>(Context&)>;

struct ResponseFrame {
    uint16_t flags = 0;
    std::vector<std::byte> payload;
};
```

### Context API (available inside a handler)

```cpp
const FrameHeader& header() const;           // Incoming frame header
bytes payload() const;                        // Raw payload bytes
Session& session() const;                     // Current session (persistent data)
TransportType current_transport() const;      // TCP / UDP / UNIX
Task<void> push_event(uint16_t cmd, bytes);   // Send unsolicited frame to client

// Property bag — pass data between authenticator and handler
template<typename T> void set(const std::string& key, T&& value);
template<typename T> T get(const std::string& key) const;        // throws if missing
template<typename T> const T* get_if(const std::string& key) const;  // nullptr if missing
```

### Session API

```cpp
uint64_t id() const;
bool is_authenticated() const;
void set_authenticated(bool state, std::string user_identifier);
const std::string& user_identifier() const;
```

### Server Operations

```cpp
Task<void> send_to(uint64_t session_id, uint16_t command_id, bytes payload);
Task<void> broadcast(uint16_t command_id, bytes payload);
Task<void> broadcast_if(uint16_t command_id, bytes payload,
                        std::function<bool(const Session&)> predicate);
void add_periodic_task(std::chrono::milliseconds interval,
                       std::function<Task<void>(Server&)> handler);
```

### Lifecycle

```cpp
void init();   // Bind sockets, start accept loops, initialize defaults
void run();    // Enter io_uring event loop (blocking)
void stop();   // Signal the loop to exit
```

---

## Custom Authentication

Implement `IAuthenticator` to inject your own auth strategy:

```cpp
class MyAuthenticator : public servd::IAuthenticator {
public:
    servd::Task<bool> authenticate(servd::Context& ctx) override {
        // Inspect session, IP, payload, or external service
        bool ok = co_await my_auth_check(ctx);
        if (ok) {
            ctx.set("custom_data", MyData{...});  // Pass data to handler
        }
        co_return ok;
    }
};

// In main():
app.set_authenticator(std::make_shared<MyAuthenticator>());
```

---

## Custom Session Store

Implement `ISessionStore` to persist sessions across restarts:

```cpp
class MySessionStore : public servd::ISessionStore {
    servd::Task<servd::Session> get_or_create(uint64_t id) override;
    servd::Task<void> save(const Session& session) override;
};

// In main():
app.set_session_store(std::make_shared<MySessionStore>());
```

---

## Config File Format

```ini
# servd.conf
tcp = 8080
tcp = 9090
udp = 8081
unix = /tmp/servd.sock
log_level = INFO
log_file = /var/log/servd.log
```

Load with `app.load_config("config/servd.conf")`.

---

## Network Discovery (optional)

servd can broadcast its presence on the LAN using UDP so clients find it without manual configuration:

```cpp
app.enable_discovery({
    .broadcast_port = 9999,
    .magic_number = 0x53525644,  // "SRVD"
    .respond_to_clients = true,
    .active_announce_if_idle = std::chrono::seconds(30)
});
```

---

## Project Structure

```
servd/
├── CMakeLists.txt              # Build configuration
├── cmake/CPM.cmake             # CPM dependency manager
├── config/servd.conf           # Example config file
├── include/servd/              # Public API headers
│   ├── Server.hpp              # Main server class
│   ├── Protocol.hpp            # Frame header, enums, discovery packets
│   ├── Context.hpp             # Per-request context with property bag
│   ├── Task.hpp                # Coroutine task<T> / task<void>
│   ├── crypto/
│   │   ├── AesGcm.hpp           # AES-256-GCM (via Botan)
│   │   ├── X25519.hpp           # X25519 key agreement (via Botan)
│   │   └── Rng.hpp              # CSPRNG (getrandom)
│   ├── auth/
│   │   └── DefaultAuthenticator.hpp
│   ├── store/
│   │   └── InMemorySessionStore.hpp
│   ├── router/
│   │   ├── Router.hpp          # O(1) array-based router
│   │   └── Endpoint.hpp        # Handler + auth/transport constraints
│   └── interfaces/
│       ├── IAuthenticator.hpp
│       ├── IConnection.hpp
│       ├── ISessionStore.hpp
│       └── Session.hpp
├── src/
│   ├── Server.cpp              # Server implementation
│   ├── UringEngine.cpp         # io_uring event loop + coroutine engine
│   └── detail/Engine.hpp       # Internal engine structures
├── Logger/                     # ANSI-colored logging utility
├── app/main.cpp                # Example application
├── examples/client_test.py     # Python test client
└── tests/                      # (future)
```

---

## Requirements

- **OS:** Linux ≥ 5.1 (for `io_uring`)
- **Compiler:** GCC ≥ 11 or Clang ≥ 14 (C++20 coroutines)
- **Library:** `liburing` (development headers), Botan ≥ 3.0 (`apt install libbotan-3-dev`)
- **Build:** CMake ≥ 4.2
- **Runtime:** `CAP_NET_ADMIN` capability (for discovery broadcast)

---

## License

MIT — see LICENSE file.
