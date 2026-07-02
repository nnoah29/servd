# servd : M2M Asynchronous Server Framework

**servd** est un framework serveur haute performance écrit en C++20. 
Conçu pour les communications Machine-to-Machine (M2M), l'IoT et les systèmes distribués, il exploite **`io_uring`** et les **Coroutines C++20** pour offrir une latence minimale et un débit réseau maximal (Zero-Copy).

L'objectif de `servd` est de séparer strictement la plomberie système (Réseau, Sockets, Polling, Multithreading) de la logique métier de l'application.

## 1. Philosophie de conception (IoC)

Contrairement aux applications systèmes monolithiques, `servd` agit comme une **boîte noire robuste** :
1. Le Framework gère le réseau (`io_uring`), les timers natifs, la découverte réseau et l'état des sessions.
2. L'Utilisateur final (Développeur de l'application) "injecte" sa logique métier via une API de type *Fluent* sans jamais manipuler le moindre File Descriptor.

## 2. Abstraction du Transport (TCP / UDP / UNIX)

Les systèmes modernes nécessitent de gérer différentes contraintes :
* **TCP / UNIX Sockets** : Pour la fiabilité et les flux lourds.
* **UDP** : Pour la résilience à la mobilité (changement d'IP/Wifi) et la latence temps réel.

Le framework gère ces transports de façon unifiée sur **la même boucle `io_uring`**. Le développeur impose ses contraintes par route :
```cpp
// Rejet automatique par le framework si reçu en UDP
app.add_command(CMD_FIRMWARE, handler).tcp_only(); 
```

## 3. Le Protocole Binaire Universel (Zéro-Parsing)

Pour éviter le goulot d'étranglement du texte (HTTP/JSON), `servd` utilise un protocole binaire (Framing) strictement aligné de 16 octets :

| Type | Champ | Taille | Description |
| :--- | :--- | :--- | :--- |
| `uint16_t` | `command_id` | 2 bytes | L'identifiant de la route (Routage en O(1)). |
| `uint16_t` | `flags` | 2 bytes | Méta-données (ex: Compressed, Encrypted). |
| `uint32_t` | `payload_length` | 4 bytes | Taille exacte des données métier en octets. |
| `uint64_t` | `session_id` | 8 bytes | Maintien de l'état (survit aux reconnexions UDP/TCP). |

Le payload est lu par `io_uring` et transmis au métier via un `std::span`. **Zéro copie, zéro parsing.**

## 4. Découverte Réseau (Zero-Configuration / Plug & Play)

Pour faciliter le déploiement sur réseaux locaux, `servd` intègre un mécanisme de découverte basé sur UDP Broadcast, utilisant un mini-protocole hors-bande de 9 octets.

* **Mode Passif :** Le serveur écoute sur un port UDP Broadcast. Quand un client émet un paquet `CLIENT_LOOKING_FOR_SERVER`, `servd` lui répond de manière asynchrone avec ses ports TCP/UDP actifs.
* **Mode Actif (Beaconing) :** Si configuré via `active_announce_if_idle`, le serveur vérifie périodiquement s'il a des clients. S'il est seul, il diffuse activement sa présence (`SERVER_ANNOUNCING`) sur tout le réseau local (`255.255.255.255`).

## 5. Routage O(1)

Le `command_id` est utilisé comme **index direct** dans un tableau interne (le `BinaryRouter`). La résolution de la route s'effectue en `O(1)` (1 cycle CPU).

## 6. Coroutines C++20 : Fin du Callback Hell

Toutes les fonctions métier retournent un `Task<T>` et utilisent `co_await`. 
`servd` attache l'adresse mémoire de la coroutine au paramètre `user_data` de `liburing` (`SQE`). Lorsque l'E/S est prête, la coroutine reprend où elle s'était arrêtée. Un seul thread peut ainsi gérer des centaines de milliers de clients sans Mutex ni blocage.

## 7. Fonctionnalités M2M Avancées

* **Push Bidirectionnel :** Le serveur peut "pousser" des notifications au client pendant l'exécution d'une tâche via `ctx.push_event()`.
* **Broadcast :** Envoi massif (`server.broadcast()`) ou ciblé (`server.broadcast_if()`) à la volée.
* **Tâches périodiques (Timers) :** via `add_periodic_task()`, basées sur l'opération native `IORING_OP_TIMEOUT` du noyau Linux.

## 8. Exemple d'utilisation (L'expérience Développeur)

```cpp
#include <servd/Server.hpp>

enum Commands : uint16_t { CMD_LOGIN = 0x01, CMD_TELEMETRY = 0x02 };

int main() {
    servd::Server app;
    app.enable_tcp(8080).enable_udp(8080);
    app.enable_discovery({.broadcast_port = 9999, .magic_number = 0x53525644}); // 'SRVD'

    // 1. Définition des routes
    app.add_command(CMD_LOGIN, [](servd::Context& ctx) -> Task<servd::ResponseFrame> {
        ctx.session().set_authenticated(true, "device_007");
        co_return servd::ResponseFrame{0, {}};
    });

    app.add_command(CMD_TELEMETRY, [](servd::Context& ctx) -> Task<servd::ResponseFrame> {
        // Traitement Zéro-Copie du ctx.payload()
        co_return servd::ResponseFrame{0, { /* ACK */ }};
    }).require_auth().udp_only();

    // 2. Tâches asynchrones (Timers sans thread bloqué)
    app.add_periodic_task(std::chrono::seconds(5), [](servd::Server& s) -> Task<void> {
        co_await s.broadcast(0x99, { /* System stats binaire */ });
    });

    // 3. Boucle io_uring
    app.init();
    app.run();
}
```
