# Définir et router des commandes

Le système de commandes est le cœur de servd. Chaque message entrant est routé vers un handler selon son `command_id` (uint16_t).

## Enregistrer une commande

```cpp
Server app;

app.add_command(0x01, [](Context& ctx) -> Task<ResponseFrame> {
    // ctx.header()      → const FrameHeader&
    // ctx.payload()     → bytes (span<const byte>)
    // ctx.session()     → Session&
    // ctx.current_transport() → TransportType
    return { { 0, /* payload */ } };
});
```

## Contraintes

```cpp
app.add_command(CMD_SECRET, handler)
    .require_auth();          // ← nécessite une session authentifiée
    .tcp_only();              // ← accessible uniquement en TCP
    // .udp_only() aussi disponible
```

## Utiliser le Context

```cpp
// Lire la requête
const FrameHeader& hdr = ctx.header();
uint16_t cmd = hdr.command_id;
uint64_t sid = hdr.session_id;
bytes data = ctx.payload();

// Écrire dans le property bag (passe-plat entre authenticator et handler)
ctx.set<std::string>("role", "admin");
auto role = ctx.get_if<std::string>("role");

// Envoyer une notification push au client
ctx.push_event(CMD_ALERT, bytes);
```

## Réponse

Le handler retourne un `ResponseFrame` :

```cpp
struct ResponseFrame {
    uint16_t flags;
    std::vector<std::byte> payload;
};
```

construit via `return { { flags, payload_vector } }`.

## Noms de commandes (mode texte)

Pour le protocole texte, vous pouvez associer des noms aux IDs :

```cpp
app.add_command_name("PING", CMD_PING);
app.add_command_name("LOGIN", CMD_LOGIN);
```

Le client texte peut alors envoyer `PING 0 0\n...` au lieu de `1 0 0\n...`.
