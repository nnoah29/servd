# Étendre une fonctionnalité existante

Ce guide explique comment modifier ou étendre une fonctionnalité existante du framework sans la réécrire.

## 1. Personnaliser le traitement des commandes

### Avant : Handler simple

```cpp
app.add_command(CMD_DATA, [](Context& ctx) -> Task<ResponseFrame> {
    // Traitement direct
    co_return ResponseFrame{ { 0, process(ctx.payload()) } };
});
```

### Après : Avec middleware de logging et mesure

```cpp
app.add_command(CMD_DATA, [](Context& ctx) -> Task<ResponseFrame> {
    auto start = std::chrono::steady_clock::now();
    auto sid = ctx.header().session_id;

    LOG(INFO, "CMD_DATA de session %lu, %zu bytes", sid, ctx.payload().size());

    auto result = process(ctx.payload());

    auto elapsed = std::chrono::steady_clock::now() - start;
    LOG(DEBUG, "CMD_DATA traité en %lld ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

    co_return ResponseFrame{ { 0, std::move(result) } };
});
```

## 2. Étendre le property bag

Le `Context` supporte déjà `get<T>` / `set<T>` / `get_if<T>`. Vous pouvez stocker n'importe quel type :

```cpp
// Définir un type pour le property bag
struct UserProfile {
    std::string name;
    int level;
    std::vector<std::string> permissions;
};

// Authenticator : écrire dans le bag
ctx.set<UserProfile>("profile", UserProfile{"alice", 42, {"read", "write"}});

// Handler : lire du bag
auto profile = ctx.get<UserProfile>("profile");
LOG(INFO, "Bienvenue %s (niveau %d)", profile.name.c_str(), profile.level);
```

## 3. Modifier le comportement du routeur

Le routeur O(1) par tableau peut être étendu pour supporter des fonctionnalités supplémentaires :

### Ajouter un middleware global

Modifiez `handle_normal_command` dans `CommandRoute.cpp` pour exécuter un middleware avant le handler.

Dans `detail/Engine.hpp`, ajoutez :

```cpp
class UringEngine {
    // ...
    std::vector<std::function<Task<bool>(Context&)>> middlewares_;
public:
    void add_middleware(auto mw) { middlewares_.push_back(std::move(mw)); }
    // ...
};
```

Dans `CommandRoute.cpp` :

```cpp
// Avant d'appeler le handler
for (auto& mw : middlewares_) {
    bool ok = co_await mw(ctx);
    if (!ok) {
        // Middleware a refusé la requête
        co_return;
    }
}
```

## 4. Ajouter des flags au FrameHeader

Les `flags` (uint16_t) dans `FrameHeader` sont réservés pour un usage futur. Vous pouvez les utiliser pour des features comme :

- Compression (`FLAG_COMPRESSED = 0x01`)
- Priorité (`FLAG_HIGH_PRIORITY = 0x02`)
- Acquittement requis (`FLAG_ACK_REQUIRED = 0x04`)

```cpp
constexpr uint16_t FLAG_COMPRESSED = 0x01;

// Côté client
FrameHeader header;
header.flags |= FLAG_COMPRESSED;

// Côté serveur (dans un handler)
if (ctx.header().flags & FLAG_COMPRESSED) {
    // Décompresser le payload
}
```

## 5. Remplacer le moteur d'E/S

Si `io_uring` ne convient pas à votre cas d'usage, le pattern PIMPL permet de remplacer `UringEngine` par une autre implémentation (epoll, IOCP, etc.) :

1. Créez une nouvelle classe implémentant la même interface que `UringEngine`
2. Modifiez `Server` pour utiliser votre moteur :

```cpp
// Dans Server.hpp
class Server {
    // std::unique_ptr<UringEngine> engine_;
    std::unique_ptr<MyEngine> engine_;  // Votre implémentation
};
```

## 6. Ajouter des statistiques

Vous pouvez étendre `UringEngine` pour collecter des métriques :

```cpp
class UringEngine {
    // ...
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> total_bytes_sent_{0};
    // ...
public:
    uint64_t total_requests() const { return total_requests_; }
    uint64_t total_bytes_sent() const { return total_bytes_sent_; }
};
```

Puis exposez via `Server` :

```cpp
class Server {
public:
    uint64_t total_requests() const { return engine_->total_requests(); }
};
```

## 7. Bonnes pratiques

- **Ne modifiez pas les headers publics** si vous pouvez étendre via injection (Strategy)
- **Utilisez le property bag** plutôt que d'ajouter des champs à `Context`
- **Gardez la compatibilité ascendante** : ajoutez des paramètres avec des valeurs par défaut
- **Documentez les changements** dans `TODO.md` et mettez à jour les tests
