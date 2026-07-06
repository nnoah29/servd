# Stockage des sessions

Le stockage des sessions utilise le **Strategy pattern** pour permettre différentes stratégies de persistance.

## Interface `ISessionStore`

```cpp
#include <servd/interfaces/ISessionStore.hpp>

namespace servd {
    class ISessionStore {
    public:
        virtual ~ISessionStore() = default;
        virtual Task<Session> get_or_create(uint64_t session_id) = 0;
        virtual Task<void> save(const Session& session) = 0;
    };
}
```

- **`get_or_create`** : Retourne la session existante pour `session_id`, ou en crée une nouvelle si elle n'existe pas (ou si `session_id == 0`).
- **`save`** : Persiste l'état de la session après chaque requête.

## InMemorySessionStore (défaut)

```cpp
#include <servd/store/InMemorySessionStore.hpp>

class InMemorySessionStore : public ISessionStore {
    std::unordered_map<uint64_t, Session> sessions_;
    // get_or_create : insert si absent
    // save : insert_or_assign
};
```

**Limitations** :
- Les sessions sont perdues au redémarrage du serveur
- Pas de partage entre plusieurs instances du serveur
- Pas de limite de mémoire (les sessions s'accumulent)

## Créer un store personnalisé

```cpp
class RedisSessionStore : public servd::ISessionStore {
public:
    Task<Session> get_or_create(uint64_t session_id) override {
        if (session_id == 0) {
            // Nouvelle session
            static std::atomic<uint64_t> next_id{1};
            return Session{next_id++};
        }
        // Récupérer depuis Redis
        Session s = /* redis.get(session_id) */;
        co_return s;
    }

    Task<void> save(const Session& session) override {
        // Persister dans Redis
        // redis.set(session.id(), session);
        co_return;
    }
};
```

## Injection

```cpp
auto store = std::make_shared<RedisSessionStore>();
app.set_session_store(store);
```

## Notes importantes

1. **Thread safety** : Toutes les opérations se déroulent sur le même thread. Si votre store accède à une ressource externe, assurez-vous qu'elle est thread-safe ou que l'accès est sérialisé.

2. **Performance** : `save()` est appelée après **chaque** requête. Si l'opération de sauvegarde est lente (I/O disque, réseau), elle impactera le débit du serveur.

3. **Session ID = 0** : Utilisée par les nouveaux clients. Le store doit attribuer un nouvel ID.

4. **Durée de vie** : Les sessions survivent à la déconnexion du client, ce qui permet la reprise de session.
