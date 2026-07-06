# Système de routage

Le routage des commandes est l'un des piliers de servd. Il est conçu pour être **le plus rapide possible** : O(1) dans tous les cas, sans hash, sans allocation.

## Architecture

Le système de routage repose sur deux classes :

```
Router (tableau fixe de 65536 Endpoint)
  │
  └── Endpoint[0]        Handler, requires_auth, allowed_transport
  └── Endpoint[1]        Handler, requires_auth, allowed_transport
  └── ...
  └── Endpoint[65535]    Handler, requires_auth, allowed_transport
```

## `Router` (include/servd/router/Router.hpp)

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

- **`add(command_id)`** : Retourne une référence à `routes_[command_id]` pour configuration. O(1).
- **`get(command_id)`** : Retourne un pointeur vers l'`Endpoint` ou `nullptr` si le handler n'est pas défini. O(1).

L'indexation directe par `uint16_t` signifie que l'espace des IDs de commande est limité à 65536 valeurs (0–65535). C'est volontaire : les IDs sont de type `uint16_t` dans le protocole filaire.

## `Endpoint` (include/servd/router/Endpoint.hpp)

```cpp
namespace servd {
    using Handler = std::function<Task<ResponseFrame>(Context&)>;

    class Endpoint {
    public:
        Handler handler = nullptr;
        bool requires_auth = false;
        TransportType allowed_transport = TransportType::ANY;

        Endpoint& require_auth();       // requires_auth = true
        Endpoint& tcp_only();           // allowed_transport = TCP
        Endpoint& udp_only();           // allowed_transport = UDP
        bool is_valid() const;          // handler != nullptr
    };
}
```

Chaque `Endpoint` contient :
- **`handler`** : La fonction de callback qui sera appelée pour traiter la commande. Signature : `Task<ResponseFrame>(Context&)`.
- **`requires_auth`** : Si `true`, l'authenticator est invoqué avant le handler. La commande est rejetée si l'authentification échoue.
- **`allowed_transport`** : Restreint le transport autorisé. `ANY`, `TCP`, `UDP`, ou `UNIX`.

## Enregistrement des commandes

```cpp
Server app;

// Commande simple
app.add_command(CMD_PING, [](Context& ctx) -> Task<ResponseFrame> {
    return { { 0, {} } };
});

// Commande avec contraintes (chaînage fluent)
app.add_command(CMD_SECRET, [](Context& ctx) -> Task<ResponseFrame> {
    return { { 0, ctx.payload() } };
})
.require_auth()      // ← nécessite authentification préalable
.tcp_only();         // ← accessible uniquement via TCP
```

L'ordre des contraintes n'a pas d'importance car chaque méthode retourne `Endpoint&`.

## Pipeline de routage

Lorsqu'une commande arrive (`handle_normal_command` dans `CommandRoute.cpp`), le pipeline est :

```
1. router_.get(header.command_id)
     → Si nullptr ou handler non défini → aucune réponse (ignoré silencieusement)
     → Sinon, continue

2. Création du Context
     Context ctx(header, payload, session, connection);

3. Vérification d'authentification
     Si endpoint->requires_auth:
       bool ok = co_await authenticator_->authenticate(ctx);
       Si !ok → la commande est ignorée

4. Vérification de transport
     Si endpoint->allowed_transport != ANY
       ET ctx.current_transport() != allowed_transport:
       → la commande est ignorée

5. Exécution du handler
     ResponseFrame response = co_await endpoint->handler(ctx);

6. Envoi de la réponse
     connection->send_frame(response.header, response.payload);
```

## Contrôle fin du transport

Les trois constantes disponibles :

| Constante | Valeur | Effet |
|---|---|---|
| `TransportType::ANY` | 0 | Tous les transports acceptés (défaut) |
| `TransportType::TCP` | 1 | TCP (binaire ou texte) uniquement |
| `TransportType::UDP` | 2 | UDP uniquement |
| `TransportType::UNIX` | 3 | Unix socket uniquement |
