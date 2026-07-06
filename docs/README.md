# servd — Framework serveur M2M asynchrone

servd est un framework C++20 pour construire des serveurs M2M (Machine-to-Machine) performants. Il repose sur `io_uring` et les coroutines pour offrir un débit élevé avec un modèle de programmation synchrone.

## Fonctionnalités

- **Protocole binaire** compact (16 bytes de header) — efficace pour le M2M
- **Transport TCP, UDP, Unix sockets** — unificateur, utilisable simultanément
- **Chiffrement de bout en bout** — X25519 + AES-256-GCM via Botan 3
- **Broadcast / Push** — le serveur peut envoyer des messages non sollicités
- **Tâches périodiques** — timers intégrés à la boucle d'événements
- **Mono-thread** — pas de mutex, pas de data race, pas de deadlock
- **Authentification injectable** — interface `IAuthenticator`
- **Stockage de sessions injectable** — interface `ISessionStore`
- **Routage O(1)** — tableau indexé par ID de commande

## Documentation

| Section | Description |
|---|---|
| [getting-started.md](getting-started.md) | Installation et premier serveur en 5 minutes |
| **Guide d'utilisation** | |
| [guide/commands.md](guide/commands.md) | Définir et router des commandes |
| [guide/authentication.md](guide/authentication.md) | Authentifier les clients |
| [guide/encryption.md](guide/encryption.md) | Chiffrer les échanges |
| [guide/messaging.md](guide/messaging.md) | Envoyer, broadcast, push |
| [guide/sessions.md](guide/sessions.md) | Gérer les sessions |
| [guide/discovery.md](guide/discovery.md) | Découverte réseau automatique |
| [guide/configuration.md](guide/configuration.md) | Configuration et transports |
| [protocol.md](protocol.md) | Spécification du protocole filaire |
| [integration.md](integration.md) | Intégrer servd dans votre projet CMake |
| [examples.md](examples.md) | Exemples complets |
