# Le contexte de requête : `Context`

`Context` est l'objet transmis à chaque handler de commande. Il donne accès à toutes les informations de la requête entrante ainsi qu'à des mécanismes de réponse et de stockage temporaire.

## Header

```cpp
#include <servd/Context.hpp>
```

## Création et cycle de vie

`Context` est **alloué sur la stack** dans `handle_normal_command` (pas d'allocation dynamique) :

```cpp
Context ctx(header, payload, session, connection);
ResponseFrame response = co_await endpoint->handler(ctx);
```

Il est détruit automatiquement à la sortée de `handle_normal_command`.

## Accès aux données de la requête

```cpp
// L'en-tête de la trame
const FrameHeader& hdr = ctx.header();
uint16_t cmd = hdr.command_id;
uint64_t sid = hdr.session_id;

// Le payload (span read-only)
bytes data = ctx.payload();
```

## Accès à la session

```cpp
Session& session = ctx.session();
session.is_authenticated();        // bool
session.user_identifier();         // string_view
session.set_authenticated(true, "username");
```

La session est persistée entre les requêtes via le `ISessionStore`.

## Détermination du transport courant

```cpp
TransportType t = ctx.current_transport();
switch (t) {
    case TransportType::TCP:  // connexion TCP
    case TransportType::UDP:  // datagramme UDP
    case TransportType::UNIX: // socket Unix
}
```

## Envoi de messages non sollicités (push)

Le handler peut envoyer un message au client sans attendre sa prochaine requête :

```cpp
ctx.push_event(CMD_NOTIFICATION, std::vector<std::byte>{...});
```

C'est utile pour les notifications, les alertes, ou tout événement asynchrone lié à ce client.

## Property bag (stockage temporaire)

Le `Context` contient un dictionnaire `std::unordered_map<std::string, std::any>` permettant de passer des données entre l'authenticator et le handler, ou entre middlewares.

```cpp
// Authenticator
ctx.set<std::string>("user_role", "admin");

// Handler (même Context, même requête)
auto role = ctx.get<std::string>("user_role");    // "admin"
auto maybe = ctx.get_if<std::string>("user_role"); // std::optional
```

Attention : Le property bag est détruit avec le `Context` à la fin du traitement.

## Exemple complet

```cpp
app.add_command(CMD_GET_STATUS, [](Context& ctx) -> Task<ResponseFrame> {
    // 1. Lire la requête
    auto transport = ctx.current_transport();
    auto session_id = ctx.header().session_id;

    // 2. Accéder à la session
    auto& session = ctx.session();
    LOG(INFO, "Requête de %s", session.user_identifier().data());

    // 3. Utiliser le property bag (données mises par l'authenticator)
    auto role = ctx.get_if<std::string>("role");
    if (role && *role == "admin") {
        // Traitement spécial admin
    }

    // 4. Construire la réponse
    std::vector<std::byte> payload = { /* ... */ };
    return { { 0, payload } };
});
```
