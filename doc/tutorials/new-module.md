# Ajouter un nouveau module

Ce guide explique comment ajouter un module fonctionnel à servd. Un module est un ensemble de classes et de fonctions qui étendent les capacités du framework.

## Structure d'un module

Un module se compose généralement de :
1. Un **header public** dans `include/servd/<module>/` (si l'API est exposée)
2. Une **implémentation** dans `src/<module>/` (optionnelle, si complexe)
3. L'**intégration** dans le CMakeLists.txt

## Exemple : Module de rate limiting

### 1. Header public

Créez `include/servd/ratelimit/RateLimiter.hpp` :

```cpp
#pragma once
#include <cstdint>
#include <chrono>
#include <unordered_map>

namespace servd {

struct RateLimitConfig {
    size_t max_requests;
    std::chrono::milliseconds window;
};

class RateLimiter {
    struct Entry {
        size_t count = 0;
        std::chrono::steady_clock::time_point reset_at;
    };

    std::unordered_map<uint64_t, Entry> entries_;
    RateLimitConfig config_;

public:
    explicit RateLimiter(RateLimitConfig config);

    // Retourne true si la requête est autorisée, false si rate limit atteint
    bool allow(uint64_t session_id);

    void reset(uint64_t session_id);
};

} // namespace servd
```

### 2. Implémentation

Créez `src/ratelimit/RateLimiter.cpp` :

```cpp
#include <servd/ratelimit/RateLimiter.hpp>

namespace servd {

RateLimiter::RateLimiter(RateLimitConfig config)
    : config_(std::move(config)) {}

bool RateLimiter::allow(uint64_t session_id) {
    auto now = std::chrono::steady_clock::now();
    auto& entry = entries_[session_id];

    if (now >= entry.reset_at) {
        entry.count = 0;
        entry.reset_at = now + config_.window;
    }

    if (entry.count >= config_.max_requests)
        return false;

    ++entry.count;
    return true;
}

void RateLimiter::reset(uint64_t session_id) {
    entries_.erase(session_id);
}

} // namespace servd
```

### 3. Utilisation dans un handler

```cpp
#include <servd/ratelimit/RateLimiter.hpp>

auto rate_limiter = std::make_shared<RateLimiter>(
    RateLimitConfig{ .max_requests = 10, .window = std::chrono::seconds(1) }
);

app.add_command(CMD_API_CALL, [rate_limiter](Context& ctx) -> Task<ResponseFrame> {
    auto sid = ctx.header().session_id;

    if (!rate_limiter->allow(sid)) {
        // Trop de requêtes
        ctx.push_event(CMD_ERROR, encode_string("Rate limit exceeded"));
        co_return ResponseFrame{ { 0, {} } };
    }

    // Traitement normal
    co_return ResponseFrame{ { 0, process(ctx.payload()) } };
});
```

### 4. Enregistrement dans le CMake

Si vous contribuez au framework principal, ajoutez vos sources à `CMakeLists.txt` :

```cmake
target_sources(servd_core PRIVATE
    # ... sources existantes ...
    src/ratelimit/RateLimiter.cpp
)
```

Si c'est un module externe, créez votre propre CMakeLists et liez à servd :

```cmake
add_library(my_module STATIC src/ratelimit/RateLimiter.cpp)
target_include_directories(my_module PUBLIC include)
target_link_libraries(my_module PRIVATE servd_core)
```

## Règles pour un bon module

1. **Namespace** : Utilisez `servd::<module>` ou votre propre namespace
2. **Headers** : Utilisez `#pragma once` systématiquement
3. **Dépendances** : Évitez les dépendances circulaires avec les autres modules
4. **Const-correctness** : Marquez les getters `const`
5. **Documentation** : Documentez chaque classe et méthode publique
6. **Tests** : Ajoutez des tests unitaires pour votre module
