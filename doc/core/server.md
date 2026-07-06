# La classe `Server`

La classe `Server` est le point d'entrée unique du framework. Elle suit le pattern **Builder** (interface fluide) et le pattern **PIMPL** (masquage de l'implémentation).

## Header

```cpp
#include <servd/Server.hpp>
```

## Alias

```cpp
namespace servd {
    using bytes = std::span<const std::byte>;
}
```

Toutes les références à des données binaires non modifiables se font via `bytes` — une vue `std::span`, pas une copie.

## Structures de configuration

### `DiscoveryConfig`

```cpp
struct DiscoveryConfig {
    uint16_t broadcast_port;         // Port UDP pour la découverte
    uint32_t magic_number;           // Identifiant magic du réseau
    bool respond_to_clients;         // Répondre aux requêtes de découverte
    bool active_announce_if_idle;    // Annonces actives même sans requête
};
```

### `PeriodicTaskInfo`

```cpp
struct PeriodicTaskInfo {
    std::chrono::milliseconds interval;
    PeriodicTaskHandler handler;
};
```

## Cycle de vie

```
Server app;
  │
  ├── Configuration (fluent)
  │     app.enable_tcp(port, mode)
  │     app.enable_udp(port)
  │     app.enable_unix_socket(path, mode)
  │     app.set_authenticator(ptr)
  │     app.set_session_store(ptr)
  │     app.set_max_clients(n)
  │     app.enable_discovery(config)
  │     app.load_config("config/servd.conf")
  │     app.add_command(id, handler)
  │     app.add_command_name(name, id)
  │     app.add_periodic_task(interval, handler)
  │
  ├── Initialisation
  │     app.init()
  │       → Crée les stores par défaut si non injectés
  │       → Bind les sockets TCP/UDP/Unix
  │       → Lance les boucles d'acceptation
  │       → Lance les boucles UDP
  │       → Lance les tâches périodiques
  │
  ├── Exécution (bloquant)
  │     app.run()
  │       → Boucle io_uring (mono-thread)
  │       → Bloque jusqu'à SIGINT/SIGTERM
  │
  └── Arrêt
        app.stop()
          → running = false
          → Sortie de la boucle io_uring
          → Fermeture des sockets
```

## Configuration fluide

Toutes les méthodes de configuration retournent `Server&` pour le chaînage :

```cpp
Server app;
app.enable_tcp(8080, ProtocolMode::BINARY)
   .enable_udp(8081)
   .set_max_clients(100)
   .add_command(CMD_PING, ping_handler);
```

## Injection de dépendances

L'authentification et le stockage des sessions sont injectables via des `shared_ptr` :

```cpp
auto my_auth = std::make_shared<MyAuthenticator>();
auto my_store = std::make_shared<MySessionStore>();
app.set_authenticator(my_auth)
   .set_session_store(my_store);
```

Si non injectés, `Server` utilise `DefaultAuthenticator` et `InMemorySessionStore`.

## Gestion des tâches périodiques

```cpp
app.add_periodic_task(
    std::chrono::seconds(30),
    [](Server& srv) -> Task<void> {
        LOG(INFO, "Tick toutes les 30 secondes");
        co_await srv.broadcast(CMD_TICK, {});
    }
);
```

## Gestion des commandes nommées (mode texte)

En mode texte, les commandes peuvent être référencées par leur nom :

```cpp
app.add_command_name("PING", CMD_PING);
app.add_command_name("LOGIN", CMD_LOGIN);
// Le client texte envoie "PING 0\n..." au lieu de "1 0\n..."
```

## Fichier de configuration

```cpp
app.load_config("config/servd.conf");
```

Le fichier utilise le format `key=value` (une ligne par directive) :

```ini
tcp=8080
udp=8081
unix=/tmp/servd.sock
max_clients=100
log_level=INFO
log_file=/var/log/servd.log
```
