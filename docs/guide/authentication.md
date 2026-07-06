# Authentification

servd utilise le pattern Strategy pour l'authentification : l'interface `IAuthenticator` est injectable.

## Par défaut

```cpp
// DefaultAuthenticator : vérifie simplement que session.is_authenticated() == true
// Utilisé automatiquement si vous n'injectez rien
```

Pour qu'une commande nécessite l'authentification :

```cpp
app.add_command(CMD_ADMIN, handler)
    .require_auth();
```

Sans `.require_auth()`, la commande est accessible à tous.

## Authenticator personnalisé

Implémentez `IAuthenticator` :

```cpp
#include <servd/interfaces/IAuthenticator.hpp>

class MyAuth : public servd::IAuthenticator {
public:
    Task<bool> authenticate(servd::Context& ctx) override {
        auto payload = ctx.payload();
        std::string pwd(reinterpret_cast<const char*>(payload.data()), payload.size());

        if (pwd == "secret") {
            ctx.session().set_authenticated(true, "admin");
            ctx.set<std::string>("role", "admin"); // stocké dans le property bag
            co_return true;
        }
        co_return false;
    }
};
```

Injection :

```cpp
app.set_authenticator(std::make_shared<MyAuth>());
```

## Flux typique

1. Client envoie `CMD_LOGIN` avec credentials → handler LOGIN appelle `session.set_authenticated(true, "user")`
2. Client envoie `CMD_ADMIN` (avec `.require_auth()`) → framework appelle `authenticator_->authenticate(ctx)` avant le handler
3. Si l'authenticator retourne `false`, la commande est ignorée silencieusement
