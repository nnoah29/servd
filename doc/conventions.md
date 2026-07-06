# Conventions de développement

## Langage et standard

- **C++20** obligatoire (coroutines, span, concepts, etc.)
- Pas de RTTI sauf dans `std::any` (déjà utilisé dans `Context`)
- Pas d'exceptions sur les chemins critiques (utilisées uniquement pour les erreurs d'E/S)

## Style de code

### Nommage

| Élément | Convention | Exemple |
|---|---|---|
| Namespace | `snake_case` | `servd` |
| Classes | `PascalCase` | `UringEngine`, `FrameHeader` |
| Fonctions/Méthodes | `snake_case` | `send_frame`, `get_or_create` |
| Variables | `snake_case` | `server_fd`, `payload_length` |
| Constantes | `UPPER_SNAKE_CASE` | `CMD_KEY_EXCHANGE`, `KEY_SIZE` |
| Macros | `UPPER_SNAKE_CASE` | `LOG`, `LOGS` |
| Templates | `T`, `U`, ou noms descriptifs | `Task<T>`, `gen<N>` |
| Fichiers | `PascalCase.hpp` | `Server.hpp`, `AesGcm.hpp` |
| Dossiers | `snake_case` | `crypto/`, `router/` |

### Formatage

- **Indentation** : 4 espaces (pas de tabs)
- **Accolades** : style K&R (ouvrante sur la même ligne)
- **Ligne max** : 100 caractères
- **`*` et `&`** : collés au type (`int* p`, `const std::string& s`)

```cpp
class Server {
public:
    Server& enable_tcp(uint16_t port, ProtocolMode mode = ProtocolMode::BINARY);

private:
    std::unique_ptr<UringEngine> engine_;
};
```

### Headers

```cpp
#pragma once  // Toujours en premier, pas de guards traditionnels

#include <standard>   // Headers standards d'abord
#include <library>    // Bibliothèques externes (Botan, liburing)

#include <servd/...>  // Headers du projet
```

### Organisation des includes

1. Headers standards
2. Bibliothèques externes
3. Headers du projet (chemin relatif depuis `include/`)

### Documentation

```cpp
/// Brève description de la classe/méthode
/// 
/// Détails supplémentaires si nécessaire.
/// Les paramètres et retours sont documentés inline.
```

Toute classe ou méthode publique dans `include/servd/` doit être documentée.

## Architecture

### Pattern PIMPL

Toute classe publique avec une implémentation complexe doit utiliser le PIMPL :

```cpp
// Server.hpp (public)
class Server {
    class UringEngine;                   // Forward declaration
    std::unique_ptr<UringEngine> engine_; // Opaque pointer
};

// Engine.hpp (interne)
class Server::UringEngine {
    // Tous les détails ici
};
```

### Strategy pattern

Les fonctionnalités extensibles utilisent des interfaces :

```cpp
class IAuthenticator {
public:
    virtual ~IAuthenticator() = default;
    virtual Task<bool> authenticate(Context& ctx) = 0;
};
```

### Fluent interface

Les méthodes de configuration retournent `*this` (référence) :

```cpp
Server& enable_tcp(uint16_t port, ProtocolMode mode);
Server& set_max_clients(size_t max_clients);
```

## Erreurs et exceptions

| Situation | Mécanisme |
|---|---|
| Erreur d'E/S io_uring | `std::system_error` (catché dans `await_resume`) |
| Erreur de configuration | `LOG(ERROR, ...)` + sortie du programme |
| Handler utilisateur | Exception propagée → déconnexion du client |
| Timeout io_uring | `-ETIME` géré silencieusement |

## Performance

- Pas d'allocation dynamique sur le chemin critique
- Pas de copies inutiles (`bytes = std::span`, pas `std::vector`)
- Pas de mutex sur le chemin chaud (mono-thread)
- `const` partout où possible

## Tests

- Les tests sont prioritaires pour v0.2
- Pas de framework de test pour l'instant
- Les tests manuels via `examples/client_test.py` et `examples/test_encryption.py` doivent passer
