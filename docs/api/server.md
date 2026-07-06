# API : `Server`

## Header

```cpp
#include <servd/Server.hpp>
```

## Alias

```cpp
using bytes = std::span<const std::byte>;
```

## Structures

### `DiscoveryConfig`

```cpp
struct DiscoveryConfig {
    uint16_t broadcast_port;
    uint32_t magic_number;
    bool respond_to_clients;
    bool active_announce_if_idle;
};
```

### `PeriodicTaskInfo`

```cpp
struct PeriodicTaskInfo {
    std::chrono::milliseconds interval;
    PeriodicTaskHandler handler;
};
```

## Type alias

```cpp
using PeriodicTaskHandler = std::function<Task<void>(Server&)>;
```

## Classe `Server`

### Construction / Destruction

```cpp
Server();
~Server();  // = default (unique_ptr gère la destruction)
```

### Configuration des transports

```cpp
Server& enable_tcp(uint16_t port, ProtocolMode mode = ProtocolMode::BINARY);
Server& enable_udp(uint16_t port);
Server& enable_unix_socket(const std::string& path, ProtocolMode mode = ProtocolMode::BINARY);
```

### Injection de dépendances

```cpp
Server& set_session_store(std::shared_ptr<ISessionStore> store);
Server& set_authenticator(std::shared_ptr<IAuthenticator> auth);
```

### Configuration générale

```cpp
Server& set_max_clients(size_t max_clients);
Server& load_config(const std::string& path);
Server& enable_discovery(const DiscoveryConfig& config);
```

### Routage

```cpp
Endpoint& add_command(uint16_t command_id, Handler handler);
Server& add_command_name(const std::string& name, uint16_t command_id);
```

### Tâches périodiques

```cpp
Server& add_periodic_task(std::chrono::milliseconds interval, PeriodicTaskHandler handler);
```

### Messagerie

```cpp
Task<void> send_to(uint64_t session_id, uint16_t command_id, bytes payload);
Task<void> broadcast(uint16_t command_id, bytes payload);
Task<void> broadcast_if(uint16_t command_id, bytes payload,
                        std::function<bool(const Session&)> predicate);
```

### Cycle de vie

```cpp
void init();
void run();    // Bloquant, mono-thread
void stop();
```

### Accès aux membres internes

```cpp
Router& router_;                              // Routeur O(1)
std::shared_ptr<ISessionStore> session_store_;
std::shared_ptr<IAuthenticator> authenticator_;
// ... membres privés voir Server.hpp
```

## Exemple complet

```cpp
#include <servd/Server.hpp>
#include <servd/Protocol.hpp>

using namespace servd;

constexpr uint16_t CMD_ECHO = 0x01;

int main() {
    Server app;

    app.enable_tcp(8080)
       .enable_udp(8081)
       .set_max_clients(50);

    app.add_command(CMD_ECHO, [](Context& ctx) -> Task<ResponseFrame> {
        co_return ResponseFrame{ { 0, {ctx.payload().begin(), ctx.payload().end()} } };
    });

    app.init();
    app.run();
}
```
