# API : Interfaces

## `IAuthenticator`

**Header** : `#include <servd/interfaces/IAuthenticator.hpp>`

```cpp
namespace servd {
    class IAuthenticator {
    public:
        virtual ~IAuthenticator() = default;
        virtual Task<bool> authenticate(Context& ctx) = 0;
    };
}
```

### Implémentation par défaut : `DefaultAuthenticator`

**Header** : `#include <servd/auth/DefaultAuthenticator.hpp>`

```cpp
class DefaultAuthenticator : public IAuthenticator {
    Task<bool> authenticate(Context& ctx) override {
        co_return ctx.session().is_authenticated();
    }
};
```

### Exemple : Authenticator basique par mot de passe

```cpp
class PasswordAuthenticator : public servd::IAuthenticator {
    std::string expected_password_;
public:
    PasswordAuthenticator(std::string password) : expected_password_(std::move(password)) {}

    Task<bool> authenticate(servd::Context& ctx) override {
        auto payload = ctx.payload();
        std::string pwd(reinterpret_cast<const char*>(payload.data()), payload.size());

        if (pwd == expected_password_) {
            ctx.session().set_authenticated(true, "user");
            co_return true;
        }
        co_return false;
    }
};
```

---

## `ISessionStore`

**Header** : `#include <servd/interfaces/ISessionStore.hpp>`

```cpp
namespace servd {
    class ISessionStore {
    public:
        virtual ~ISessionStore() = default;
        virtual Task<Session> get_or_create(uint64_t session_id) = 0;
        virtual Task<void> save(const Session& session) = 0;
    };
}
```

### Implémentation par défaut : `InMemorySessionStore`

**Header** : `#include <servd/store/InMemorySessionStore.hpp>`

```cpp
class InMemorySessionStore : public ISessionStore {
    std::unordered_map<uint64_t, Session> sessions_;
public:
    Task<Session> get_or_create(uint64_t session_id) override {
        auto [it, inserted] = sessions_.try_emplace(session_id);
        if (inserted) {
            // Nouvelle session avec ID auto-généré
            static uint64_t next_id = 1;
            it->second = Session(next_id++);
        }
        co_return it->second;
    }

    Task<void> save(const Session& session) override {
        sessions_.insert_or_assign(session.id(), session);
        co_return;
    }
};
```

---

## `IConnection`

**Header** : `#include <servd/interfaces/IConnection.hpp>`

```cpp
namespace servd {
    class IConnection {
    public:
        virtual ~IConnection() = default;
        virtual TransportType transport_type() const = 0;
        virtual Task<void> send_frame(const FrameHeader& header, bytes payload) = 0;
        virtual std::string get_remote_address() const = 0;
    };
}
```

### Implémentations intégrées

| Classe | Transport | Fichier source |
|---|---|---|
| `UringTcpConnection` | TCP binaire | `src/UringEngine/ConnectionImpl.cpp` |
| `TextTcpConnection` | TCP texte | `src/UringEngine/ConnectionImpl.cpp` |
| `UringUdpConnection` | UDP | `src/UringEngine/UdpConnection.cpp` |

---

## `Session`

**Header** : `#include <servd/interfaces/Session.hpp>`

```cpp
namespace servd {
    class Session {
    public:
        Session() = default;
        explicit Session(uint64_t id);

        uint64_t id() const;
        bool is_authenticated() const;
        void set_authenticated(bool state, std::string_view user_id = {});
        std::string_view user_identifier() const;

        bool has_aes_key() const;
        void set_aes_key(const std::array<uint8_t, 32>& key);
        const std::array<uint8_t, 32>& aes_key() const;
    };
}
```

### Exemple

```cpp
ctx.session().set_authenticated(true, "alice");
LOG(INFO, "Utilisateur: %s", ctx.session().user_identifier().data());
```

---

## `Endpoint`

**Header** : `#include <servd/router/Endpoint.hpp>`

```cpp
namespace servd {
    using Handler = std::function<Task<ResponseFrame>(Context&)>;

    class Endpoint {
    public:
        Handler handler = nullptr;
        bool requires_auth = false;
        TransportType allowed_transport = TransportType::ANY;

        Endpoint& require_auth();
        Endpoint& tcp_only();
        Endpoint& udp_only();
        bool is_valid() const;
    };
}
```

### Exemple

```cpp
app.add_command(CMD_ADMIN, admin_handler)
    .require_auth()
    .tcp_only();
```

---

## `Router`

**Header** : `#include <servd/router/Router.hpp>`

```cpp
namespace servd {
    class Router {
        std::array<Endpoint, 65536> routes_;
    public:
        Endpoint& add(uint16_t command_id);
        const Endpoint* get(uint16_t command_id) const;
    };
}
```

### Exemple

```cpp
Router router;
router.add(CMD_PING).handler = [](Context& ctx) -> Task<ResponseFrame> { ... };
const Endpoint* ep = router.get(CMD_PING);
if (ep && ep->is_valid()) { ... }
```
