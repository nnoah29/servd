# Sessions

Chaque client est identifié par un `session_id` (uint64_t) transmis dans chaque `FrameHeader`. servd gère automatiquement le cycle de vie des sessions.

## API de Session

```cpp
Session& session = ctx.session();

session.id();                      // uint64_t
session.is_authenticated();        // bool
session.set_authenticated(true, "alice");  // marque comme authentifié
session.user_identifier();         // string_view → "alice"

session.has_aes_key();             // bool (après key exchange)
session.set_aes_key(key);          // stocke la clé AES
session.aes_key();                 // const array<uint8_t,32>&
```

## Stockage des sessions

Par défaut, les sessions sont stockées en mémoire (`InMemorySessionStore`). Pour persister ailleurs (Redis, SQLite, fichier), implémentez `ISessionStore` :

```cpp
#include <servd/interfaces/ISessionStore.hpp>

class MyStore : public servd::ISessionStore {
    Task<Session> get_or_create(uint64_t session_id) override;
    Task<void> save(const Session& session) override;
};

app.set_session_store(std::make_shared<MyStore>());
```

Les sessions survivent à la déconnexion du client (utile pour la reprise de session).
