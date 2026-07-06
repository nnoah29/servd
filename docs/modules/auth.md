# Authentification

L'authentification utilise le **Strategy pattern** : `Server` délègue à un `IAuthenticator` injectable, avec une implémentation par défaut.

## Interface `IAuthenticator`

```cpp
#include <servd/interfaces/IAuthenticator.hpp>

namespace servd {
    class IAuthenticator {
    public:
        virtual ~IAuthenticator() = default;
        virtual Task<bool> authenticate(Context& ctx) = 0;
    };
}
```

- **Entrée** : `Context&` (accès à la session, au payload, au header, et au property bag)
- **Sortie** : `Task<bool>` — `true` si autorisé, `false` si refusé

## DefaultAuthenticator

```cpp
#include <servd/auth/DefaultAuthenticator.hpp>

// Vérifie simplement session.is_authenticated()
class DefaultAuthenticator : public IAuthenticator {
    Task<bool> authenticate(Context& ctx) override {
        co_return ctx.session().is_authenticated();
    }
};
```

**Comportement** : Si la session a été marquée authentifiée (via `session.set_authenticated(true)`), toutes les commandes avec `.require_auth()` passent. Sinon, elles sont refusées.

## Créer un authenticator personnalisé

```cpp
class MyAuthenticator : public servd::IAuthenticator {
public:
    Task<bool> authenticate(servd::Context& ctx) override {
        // 1. Lire les credentials depuis le payload
        auto payload = ctx.payload();
        std::string username = extract_username(payload);
        std::string password = extract_password(payload);

        // 2. Vérifier (ex: base de données, LDAP, fichier)
        if (verify_credentials(username, password)) {
            ctx.session().set_authenticated(true, username);

            // 3. Optionnel : stocker des données dans le property bag
            ctx.set<std::string>("role", fetch_role(username));
            co_return true;
        }

        co_return false;
    }
};
```

## Injection

```cpp
auto auth = std::make_shared<MyAuthenticator>();
app.set_authenticator(auth);
```

## Activation sur une route

```cpp
app.add_command(CMD_ADMIN_TASK, handler)
    .require_auth();  // ← IAuthenticator::authenticate() sera appelée
```

Sans `.require_auth()`, la commande est accessible sans authentification.

## Flux d'authentification

```
1. Client envoie CMD_LOGIN
2. Handler LOGIN valide les credentials
3. Handler appelle ctx.session().set_authenticated(true, "username")
4. Client envoie CMD_SECURE_DATA
5. Avant d'appeler le handler, le framework vérifie :
   - Endpoint.requires_auth == true
   - authenticator_->authenticate(ctx) co_await
   - DefaultAuthenticator → session.is_authenticated() → true
6. Le handler CMD_SECURE_DATA est exécuté
```

## Cas d'utilisation du property bag

L'authenticator peut écrire dans le property bag du `Context` :

```cpp
// Authenticator
ctx.set<std::string>("role", "admin");
ctx.set<int>("user_id", 42);

// Handler
auto role = ctx.get_if<std::string>("role");  // "admin"
auto id = ctx.get_if<int>("user_id");          // 42
```

Le property bag est détruit à la fin de la requête.
