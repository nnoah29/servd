# Gestion des sessions

Une session représente l'état persistant associé à une connexion client. Elle est stockée entre les requêtes et accessible depuis les handlers.

## Header

```cpp
#include <servd/interfaces/Session.hpp>
```

## La classe `Session`

```cpp
namespace servd {
    class Session {
        uint64_t id_;                       // Identifiant unique
        bool authenticated_ = false;        // Authentifié ?
        std::string user_identifier_;       // Identifiant utilisateur (ex: "alice")
        bool has_aes_key_ = false;          // Clé de chiffrement présente ?
        std::array<uint8_t, 32> aes_key_;   // Clé AES partagée
        // ...
    public:
        uint64_t id() const;
        bool is_authenticated() const;
        void set_authenticated(bool state, std::string_view user_id = {});
        std::string_view user_identifier() const;
        bool has_aes_key() const;
        void set_aes_key(const std::array<uint8_t, 32>& key);
        const std::array<uint8_t, 32>& aes_key() const;
    };
}
```

## Cycle de vie d'une session

```
1. Connexion client
   → session_store_->get_or_create(session_id)
   → Créée si session_id == 0 (nouveau client)
   → Sinon, restaure la session existante

2. Pendant la session
   → Le client peut s'authentifier : session.set_authenticated(true)
   → Le key exchange X25519 peut stocker la clé AES : session.set_aes_key(key)
   → Tout handler peut modifier la session

3. Après chaque requête
   → session_store_->save(session)
   → Persiste l'état pour la prochaine requête

4. Déconnexion
   → unregister_session(session_id, fd)
   → La session reste dans le store (persistance)
```

## Identifiant de session

`session_id` est un `uint64_t` transmis dans chaque `FrameHeader`. L'attribution des IDs est à la charge du client et du `ISessionStore` :

- **Session ID = 0** : nouvelle session (le store crée une nouvelle entrée)
- **Session ID ≠ 0** : reprise de session existante

## Modification de la session dans un handler

```cpp
app.add_command(CMD_LOGIN, [](Context& ctx) -> Task<ResponseFrame> {
    // Extraire les credentials du payload
    // ... validation ...
    ctx.session().set_authenticated(true, "alice");
    return { { 0, {} } };
});

app.add_command(CMD_LOGOUT, [](Context& ctx) -> Task<ResponseFrame> {
    ctx.session().set_authenticated(false);
    return { { 0, {} } };
});
```

## Stockage de sessions personnalisé

Voir [ISessionStore](../api/interfaces.md#isessionstore) et [Session Store](../modules/session-store.md) pour créer votre propre backend de persistance.

## Exemple : vérification d'authentification

```cpp
// Dans un handler
if (!ctx.session().is_authenticated()) {
    ctx.push_event(CMD_ERROR, encode_string("Authentification requise"));
    co_return ResponseFrame{ { 0, {} } };
}
// Traitement pour utilisateur authentifié
```
