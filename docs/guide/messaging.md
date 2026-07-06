# Envoyer des messages

## Depuis un handler (push)

```cpp
ctx.push_event(CMD_NOTIF, data);
```

Envoie un message non sollicité au client de cette session.

## Depuis le Server (broadcast ciblé)

```cpp
// À une session spécifique
co_await app.send_to(session_id, CMD_MSG, payload);

// À toutes les sessions
co_await app.broadcast(CMD_ALERT, payload);

// Aux sessions satisfaisant un prédicat
co_await app.broadcast_if(CMD_ALERT, payload,
    [](const Session& s) { return s.is_authenticated(); }
);
```

Ces méthodes sont des coroutines (nécessitent `co_await` ou d'être appelées depuis une coroutine).

## Depuis une tâche périodique

```cpp
app.add_periodic_task(std::chrono::seconds(10), [](Server& srv) -> Task<void> {
    co_await srv.broadcast(CMD_HEARTBEAT, encode("ping"));
});
```

Le handler reçoit une référence `Server&` pour envoyer des messages.
