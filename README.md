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

**Dependencies:** Linux ≥ 5.1, `liburing`, C++20 compiler (GCC ≥ 11 / Clang ≥ 14).

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
├── config/servd.conf           # Example config file
├── include/servd/              # Public API headers
│   ├── Server.hpp              # Main server class
│   ├── Protocol.hpp            # Frame header, enums, discovery packets
│   ├── Context.hpp             # Per-request context with property bag
│   ├── Task.hpp                # Coroutine task<T> / task<void>
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
- **Library:** `liburing` (developement headers)
- **Build:** CMake ≥ 4.2
- **Runtime:** `CAP_NET_ADMIN` capability (for discovery broadcast)

---

## License

MIT — see LICENSE file.
