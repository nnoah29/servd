# API : `Context`

## Header

```cpp
#include <servd/Context.hpp>
```

## Classe `Context`

```cpp
namespace servd {
    class Context {
    public:
        Context(const FrameHeader& header, bytes payload,
                Session& session, IConnection& connection);
        // Pas de copie (interdit)
        Context(const Context&) = delete;

        // Accès à la requête entrante
        const FrameHeader& header() const;
        bytes payload() const;

        // Accès à la session
        Session& session();

        // Accès au transport
        TransportType current_transport() const;

        // Push (message non sollicité au client)
        void push_event(uint16_t command_id, bytes data);

        // Property bag (std::any)
        template<typename T>
        void set(const std::string& key, T&& value);

        template<typename T>
        T& get(const std::string& key);              // lève std::bad_any_cast si absent

        template<typename T>
        std::optional<T> get_if(const std::string& key) const;  // std::nullopt si absent
    };
}
```

## Exemples d'utilisation

### Lecture des données de requête

```cpp
auto cmd = ctx.header().command_id;
auto session_id = ctx.header().session_id;
auto data = ctx.payload();
```

### Écriture dans le property bag

```cpp
// Dans un authenticator
ctx.set<std::string>("role", "admin");
ctx.set<int>("user_id", 42);

// Dans un handler
auto role = ctx.get<std::string>("role");
auto user_id = ctx.get_if<int>("user_id");
```

### Push event

```cpp
// Envoyer une notification asynchrone au client
std::vector<std::byte> notif = encode("Nouveau message");
ctx.push_event(CMD_NOTIFICATION, notif);
```

### Détection du transport

```cpp
if (ctx.current_transport() == TransportType::UDP) {
    // Comportement spécifique UDP
}
```
