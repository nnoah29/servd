# Le type coroutine : `Task<T>`

servd implémente son propre type coroutine C++20 sans dépendance externe. Pas de Boost.Asio, pas de `cppcoro` — une implémentation maison légère.

## Header

```cpp
#include <servd/Task.hpp>
```

## `Task<T>` — Coroutine avec valeur de retour

```cpp
template <typename T>
class Task {
    // promise_type avec:
    //   - initial_suspend : suspend_always (lazy)
    //   - final_suspend   : resume la continuation
    //   - return_value(T) : stocke la valeur
    //   - unhandled_exception : stocke l'exception
    //
    // FinalAwaiter :
    //   - await_ready() → false
    //   - await_suspend() → resumer la continuation handle
    //
    // Move only (pas de copie)
};
```

### Utilisation

```cpp
Task<int> compute(int x) {
    co_return x * 2;
}

Task<void> example() {
    int result = co_await compute(21);
    // result == 42
}
```

### Lazy evaluation

Les `Task<T>` sont **lazy** (paresseuses) : `initial_suspend::suspend_always` signifie que la coroutine ne commence pas son exécution tant qu'elle n'est pas `co_await`ée. C'est essentiel pour la composition.

## `Task<void>` — Coroutine sans valeur

Spécialisation avec `return_void()` :

```cpp
Task<void> log_and_do(Server& srv) {
    LOG(INFO, "Début");
    co_await srv.send_to(42, CMD_PING, {});
    LOG(INFO, "Fin");
    // Pas de co_return nécessaire
}
```

## `DetachedTask` — Fire-and-forget

```cpp
class DetachedTask {
    // initial_suspend : suspend_never (eager)
    // final_suspend   : suspend_never (auto-destruct)
    // Pas de valeur, pas d'exception stockée
};
```

Utilisée pour les boucles serveur qui ne sont jamais `co_await`ées :

```cpp
// Dans ClientHandlers.cpp
DetachedTask handle_client(int fd) {
    // ... boucle infinie de traitement ...
    // Ne retourne jamais ; détruite à la déconnexion
}
```

## Pont avec io_uring : `UringOperation`

La structure interne `UringOperation` (dans `detail/Engine.hpp`) est le mécanisme qui relie les coroutines à io_uring :

```cpp
struct UringOperation {
    std::coroutine_handle<> coro;
    int cqe_res;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        coro = h;  // Stocke le handle dans l'opération
        // Le SQE a déjà été préparé avec &coro comme user_data
    }

    int await_resume() const noexcept {
        // Vérifie le résultat du CQE
        if (cqe_res < 0 && cqe_res != -ETIME)
            throw std::system_error(-cqe_res, std::system_category());
        return cqe_res;
    }
};
```

Le cycle complet :

```
1. async_read(fd, buf) est appelé
2. UringOperation op est créé sur la stack de la coroutine
3. io_uring_get_sqe → sqe->user_data = &op
4. co_await op
5. await_suspend : stocke le coroutine_handle dans op.coro, suspend la coroutine
6. Quand le CQE arrive :
   - run() récupère op = (UringOperation*)cqe->user_data
   - op->cqe_res = cqe->res
   - op->coro.resume()
7. await_resume : vérifie cqe_res, le retourne
```

## Bonnes pratiques

### Toujours `co_await` les appels d'E/S

```cpp
// CORRECT
int n = co_await engine->async_read(fd, buf);

// FAUX — ne compile pas
int n = engine->async_read(fd, buf);
```

### Ne pas bloquer dans un handler

Puisque tout est mono-thread, un `sleep()` ou une boucle longue bloquerait tout le serveur. Utilisez `co_await` pour toute attente.

### Propagation d'exceptions

Les exceptions sont stockées via `promise_type::unhandled_exception` et relancées au point de `co_await` :

```cpp
Task<void> risky() {
    throw std::runtime_error("boom");
}

Task<void> safe() {
    try {
        co_await risky();
    } catch (const std::exception& e) {
        LOG(ERROR, "Récupéré: %s", e.what());
    }
}
```

### Pas de fuite mémoire

Les coroutines C++20 sont des objets heap-alloués. `Task<T>` détruit la coroutine via le `FinalAwaiter`. `DetachedTask` se détruit toute seule à la fin. Aucune fuite mémoire dans le cas normal.
