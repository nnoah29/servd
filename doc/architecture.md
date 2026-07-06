# Architecture du framework

## Philosophie

servd est construit autour de quatre principes fondamentaux :

1. **Performance maximaliste** — `io_uring` pour zéro syscall par requête, routage O(1), pas d'allocation dynamique sur le chemin critique
2. **Simplicité du modèle de concurrence** — Mono-thread avec coroutines C++20 cooperatives : pas de mutex, pas de deadlock, pas de data race
3. **Extensibilité** — Tous les composants métier (authentification, stockage, transport) sont des interfaces injectables (Strategy)
4. **Zero-copy par défaut** — Les payloads sont manipulés via `std::span` ; les copies coûteuses sont explicites

## Vue d'ensemble

```
                        Application (app/main.cpp)
                              │
                              │ add_command / set_authenticator / etc.
                              ▼
                     ┌──────────────────┐
                     │   servd::Server  │
                     │                  │
                     │  ┌─ Router ────┐ │  O(1) array<Endpoint, 65536>
                     │  │ (65536 cmd) │ │
                     │  └─────────────┘ │
                     │                  │
                     │  ISessionStore ──│── InMemorySessionStore (défaut)
                     │  IAuthenticator ─│── DefaultAuthenticator (défaut)
                     └────────┬─────────┘
                              │ owns (PIMPL)
                              ▼
                    ┌──────────────────┐
                    │   UringEngine    │  (detail/Engine.hpp)
                    │   (mono-thread)  │
                    │                  │
                    │  ┌─ io_uring ──┐ │
                    │  │ event loop  │ │
                    │  └─────────────┘ │
                    │                  │
                    │  TCP accept loops│
                    │  UDP datagram   │
                    │  Timer loops    │
                    │  Client handlers│
                    └──────────────────┘
                              │
                    ┌─────────┴──────────┐
                    ▼                    ▼
              TCP clients            UDP clients
              (binary/text)          (datagram)
```

## Composants clés

### 1. `Server` — Point d'entrée public

La classe `Server` est l'interface unique du framework. Elle expose :
- La configuration des transports (`enable_tcp`, `enable_udp`, `enable_unix_socket`)
- Le routage des commandes (`add_command`, `add_command_name`)
- Les mécanismes d'envoi (`send_to`, `broadcast`, `broadcast_if`)
- Le cycle de vie (`init`, `run`, `stop`)

`Server` utilise le pattern **PIMPL** (Pointer to Implementation) pour cacher complètement les détails du moteur `io_uring`. Le fichier d'en-tête public `Server.hpp` ne contient qu'un `unique_ptr` vers une classe forward-déclarée `UringEngine`.

### 2. `UringEngine` — Cœur asynchrone

Le `UringEngine` est le moteur d'E/S asynchrone. Il :
- Initialise une instance `io_uring` avec 256 entrées dans son constructeur
- Boucle sur `io_uring_wait_cqe` dans `run()`
- Reprend les coroutines suspendues via leur `coroutine_handle` stocké dans le champ `data` du CQE
- Gère les signaux `SIGINT`/`SIGTERM` pour un arrêt gracieux

### 3. `Router` — Routage O(1)

Le routeur est un `std::array<Endpoint, 65536>` indexé directement par `command_id`. L'ajout et la consultation sont tous deux O(1) — pas de hash, pas de tree walk.

```cpp
// Routeur : tableau indexé par ID de commande
using Router = std::array<Endpoint, 65536>;
// add(command_id) : O(1) — écrit dans routes_[command_id]
// get(command_id) : O(1) — lit routes_[command_id]
```

### 4. Coroutines et `UringOperation`

Le pont entre les coroutines C++20 et `io_uring` est assuré par la structure `UringOperation` :

```
co_await async_read(fd, buf)
  │
  ├─ UringOperation stocké sur la stack de la coroutine
  ├─ SQE préparé avec le champ user_data = &UringOperation
  └─ await_suspend() stocke le coroutine_handle dans UringOperation
      └─ Le coroutine_handle est récupéré dans la boucle d'événements
          via le CQE.user_data, puis op->cqe_res est setté et
          le handle est resumed
```

Cela permet un pattern d'écriture synchrone pour du code asynchrone :

```cpp
Task<void> handle_client(int fd) {
    // Chaque co_await suspend la coroutine sans bloquer le thread
    auto frame = co_await engine->read_frame(fd);
    // ... traiter ...
}
```

### 5. Système de transport

Trois implémentations de `IConnection` :

| Classe | Transport | Mode |
|---|---|---|
| `UringTcpConnection` | TCP | Binaire (16-byte header + payload) |
| `TextTcpConnection` | TCP | Texte (ligne de header + ligne de payload) |
| `UringUdpConnection` | UDP | Datagramme binaire |

Chaque connexion encapsule le file descriptor et implémente `send_frame()`.

## Flux de traitement d'une requête

```
Client TCP
  │
  │  [Connexion TCP établie]
  ▼
async_accept(server_fd)
  │  Retourne un nouveau fd client
  ▼
handle_client(fd)
  │  Crée UringTcpConnection
  │  Boucle :
  ▼
read_frame(fd)
  │  async_read_exact(16 bytes) → FrameHeader
  │  async_read_exact(payload_length) → payload bytes
  ▼
session_store_->get_or_create(header.session_id)
  │  Crée ou récupère la session
  ▼
register_session(session_id, fd)
  │  Ajoute à la map sessions_ (pour send_to/broadcast)
  ▼
process_command(frame, connection, session)
  │
  ├── CMD_KEY_EXCHANGE (0x00F0)
  │     → handle_key_exchange()
  │       • Vérifie payload = 32 bytes (clé publique client X25519)
  │       • Génère keypair serveur X25519
  │       • Calcule shared_secret → derive AES key
  │       • Stocke clé AES dans la session
  │       • Renvoie la clé publique serveur
  │
  ├── CMD_ENCRYPTED_MESSAGE (0x00F1)
  │     → handle_encrypted_message()
  │       • do_decrypt : extrait IV(12) + Tag(16) + ciphertext
  │       • Déchiffre avec AES-256-GCM
  │       • Extrait inner_cmd (uint16_t) et inner_payload
  │       • Route la commande interne
  │       • Chiffre la réponse et la renvoie
  │
  └── Commande normale
        → handle_normal_command()
          • router_.get(header.command_id)  → O(1)
          • Context ctx(header, payload, session, conn)
          • Si requires_auth → authenticator_->authenticate(ctx)
          • Vérifie allowed_transport
          • Appelle handler(ctx) → ResponseFrame
          • connection->send_frame(response)
  ▼
session_store_->save(session)
```

## Boucle d'événements

Le thread unique exécute la boucle suivante dans `UringEngine::run()` :

```
1. Ignorer SIGPIPE
2. Installer handler SIGINT/SIGTERM → g_signal_received = true
3. Tant que running et !g_signal_received :
   a. io_uring_wait_cqe(&ring, &cqe)  ← bloquant, pas de busy-wait
   b. UringOperation* op = (UringOperation*)cqe->user_data
   c. op->cqe_res = cqe->res
   d. op->coro.resume()  ← reprend la coroutine qui attendait
   e. io_uring_cqe_seen(&ring, cqe)
4. Nettoyage : fermeture des sockets, exit de io_uring
```

## Gestion du signal

`SIGINT` et `SIGTERM` sont capturés via `sigaction` (pas de signal-safe issues — le handler se contente d'écrire `true` dans un `std::atomic<bool>`). La boucle vérifie ce flag à chaque itération pour sortir proprement. `SIGPIPE` est ignoré (`SIG_IGN`) : les écritures sur socket fermée retournent simplement `-EPIPE` via `async_write`.

## Broadcast et push

Le serveur peut envoyer des messages de manière proactive :

- `send_to(session_id, cmd, payload)` — envoie à une session spécifique (lookup fd dans `sessions_`)
- `broadcast(cmd, payload)` — envoie à toutes les sessions
- `broadcast_if(cmd, payload, predicate)` — envoie aux sessions satisfaisant un prédicat

Le `Context` expose `push_event(cmd, data)` pour qu'un handler envoie un message non sollicité au client courant.

## Découverte réseau

Le protocole de découverte utilise des broadcasts UDP. Le serveur écoute sur un port de découverte et répond aux paquets `CLIENT_LOOKING_FOR_SERVER` avec un paquet `SERVER_ANNOUNCING` contenant le port TCP et UDP du serveur.

## Modèle de threading

**Mono-thread.** Toutes les coroutines s'exécutent sur le même thread. Il n'y a ni mutex, ni section critique, ni variable atomique sur le chemin critique. La seule variable atomique est `g_signal_received`, écrite par le handler signal et lue par la boucle principale.

Conséquences :
- Pas de data race possible
- Pas de deadlock
- Pas de coût de synchronisation
- Pas besoin de thread-safe pour les composants injectés (authenticator, session store)
