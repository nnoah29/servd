# Ajouter un nouveau composant

Ce guide explique comment ajouter un nouveau composant au framework. Un composant est une pièce d'infrastructure interne (ex: un nouveau type de transport, une nouvelle implémentation de `IAuthenticator`, etc.).

## Scénario 1 : Nouvelle implémentation d'interface

### Exemple : Authenticator à jetons (JWT)

Créez `include/servd/auth/JwtAuthenticator.hpp` :

```cpp
#pragma once
#include <servd/interfaces/IAuthenticator.hpp>
#include <string>
#include <vector>

namespace servd {

class JwtAuthenticator : public IAuthenticator {
    std::string secret_;

public:
    explicit JwtAuthenticator(std::string secret);

    Task<bool> authenticate(Context& ctx) override;
};

} // namespace servd
```

Créez `src/auth/JwtAuthenticator.cpp` :

```cpp
#include <servd/auth/JwtAuthenticator.hpp>

namespace servd {

JwtAuthenticator::JwtAuthenticator(std::string secret)
    : secret_(std::move(secret)) {}

Task<bool> JwtAuthenticator::authenticate(Context& ctx) {
    auto payload = ctx.payload();
    std::string_view token(
        reinterpret_cast<const char*>(payload.data()),
        payload.size()
    );

    // Vérifier le JWT (utiliser une bibliothèque comme jwt-cpp)
    auto decoded = verify_jwt(token, secret_);
    if (decoded) {
        ctx.session().set_authenticated(true, decoded->subject);
        ctx.set<std::string>("role", decoded->role);
        co_return true;
    }

    co_return false;
}

} // namespace servd
```

Utilisation :

```cpp
auto jwt_auth = std::make_shared<JwtAuthenticator>("mon-secret");
app.set_authenticator(jwt_auth);
```

## Scénario 2 : Nouveau type de transport

Pour ajouter un nouveau transport (ex: WebSocket, QUIC), il faut implémenter l'interface `IConnection` et ajouter la logique d'acceptation dans le `UringEngine`.

### Étapes

1. **Créez la classe de connexion** implémentant `IConnection` :

```cpp
#pragma once
#include <servd/interfaces/IConnection.hpp>

namespace servd {

class WebSocketConnection : public IConnection {
    int fd_;
    UringEngine* engine_;
    // État WebSocket (handshake, masquage, etc.)
    bool handshake_done_ = false;

public:
    WebSocketConnection(int fd, UringEngine* engine);

    TransportType transport_type() const override {
        return TransportType::TCP; // ou un nouveau type
    }

    Task<void> send_frame(const FrameHeader& header, bytes payload) override;
    std::string get_remote_address() const override;
};

} // namespace servd
```

2. **Ajoutez la boucle d'acceptation** dans `UringEngine` ou modifiez le handler client.

3. **Ajoutez la configuration** dans `Server` :

```cpp
class Server {
public:
    Server& enable_websocket(uint16_t port);
    // ...
};
```

4. **Implémentez `enable_websocket`** dans `ServerConfiguration.cpp` pour stocker la config.

5. **Ajoutez la logique `init`** dans `ServerLifecycle.cpp` pour bind le socket WebSocket et lancer la boucle d'acceptation.

## Scénario 3 : Nouvelle implémentation de `ISessionStore`

### Exemple : Session store avec fichier

```cpp
#include <servd/interfaces/ISessionStore.hpp>
#include <fstream>

class FileSessionStore : public servd::ISessionStore {
    std::string path_;
public:
    explicit FileSessionStore(std::string path) : path_(std::move(path)) {}

    Task<Session> get_or_create(uint64_t session_id) override {
        if (session_id == 0) {
            static uint64_t next = 1;
            co_return Session(next++);
        }
        // Charger depuis le fichier
        Session s;
        // ... désérialisation ...
        co_return s;
    }

    Task<void> save(const Session& session) override {
        // Sauvegarder dans le fichier
        co_return;
    }
};
```

## Règles générales

1. **Respectez les interfaces existantes** : `IAuthenticator`, `ISessionStore`, `IConnection`
2. **Utilisez le PIMPL** si votre composant a des dépendances internes lourdes
3. **Header uniquement si possible** : Pour les petits composants, le code peut rester dans le `.hpp`
4. **Documentez** : `#pragma once` + commentaires Doxygen-style
5. **Namespace** : Restez dans `servd::`
