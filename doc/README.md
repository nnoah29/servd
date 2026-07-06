# servd — Documentation

Bienvenue dans la documentation officielle de **servd**, un framework serveur asynchrone C++20 haute performance, mono-thread, basé sur `io_uring` et les coroutines.

## À propos

servd est un framework M2M (Machine-to-Machine) orienté protocole binaire, conçu pour des applications nécessitant :
- Des performances élevées grâce à `io_uring` (zero-copy, zero-syscall par requête)
- Un modèle de concurrence simple : coroutines C++20 sur une seule thread (pas de mutex, pas de data race)
- Une architecture extensible via l'injection de dépendances (Strategy pattern)
- Un chiffrement de bout en bout intégré (X25519 + AES-256-GCM)
- Un routage des commandes en O(1) par tableau indexé

## Structure de la documentation

| Section | Description |
|---|---|
| [**getting-started.md**](getting-started.md) | Installation, compilation, premier serveur |
| [**architecture.md**](architecture.md) | Architecture générale, philosophie, flux de données |
| [**project-structure.md**](project-structure.md) | Organisation du code source et rôle de chaque dossier |
| **core/** | Concepts fondamentaux |
| - [core/server.md](core/server.md) | La classe `Server` : configuration, cycle de vie |
| - [core/routing.md](core/routing.md) | Système de routage : `Router` et `Endpoint` |
| - [core/context.md](core/context.md) | Le contexte de requête : `Context` et son sac de propriétés |
| - [core/task.md](core/task.md) | Le type `Task<T>` : coroutines C++20 |
| - [core/protocol.md](core/protocol.md) | Protocole filaire : `FrameHeader`, modes binaire/texte |
| - [core/session.md](core/session.md) | Gestion des sessions |
| **modules/** | Modules fonctionnels |
| - [modules/transport.md](modules/transport.md) | Transports : TCP, UDP, Unix sockets |
| - [modules/crypto.md](modules/crypto.md) | Chiffrement : X25519, AES-256-GCM |
| - [modules/auth.md](modules/auth.md) | Authentification : interface et implémentations |
| - [modules/session-store.md](modules/session-store.md) | Stockage des sessions : interface et implémentations |
| - [modules/logging.md](modules/logging.md) | Journalisation |
| - [modules/discovery.md](modules/discovery.md) | Découverte réseau automatique |
| **api/** | Référence exhaustive de l'API publique |
| - [api/server.md](api/server.md) | `Server` — API complète |
| - [api/protocol.md](api/protocol.md) | `FrameHeader`, `TransportType`, `ProtocolMode` |
| - [api/context.md](api/context.md) | `Context` — API complète |
| - [api/interfaces.md](api/interfaces.md) | `IAuthenticator`, `ISessionStore`, `IConnection` |
| - [api/crypto.md](api/crypto.md) | `AesGcm`, `X25519`, `Rng` |
| **tutorials/** | Guides pas à pas |
| - [tutorials/installation.md](tutorials/installation.md) | Installer le framework |
| - [tutorials/new-project.md](tutorials/new-project.md) | Créer un nouveau projet |
| - [tutorials/new-module.md](tutorials/new-module.md) | Ajouter un nouveau module |
| - [tutorials/new-component.md](tutorials/new-component.md) | Ajouter un nouveau composant |
| - [tutorials/extend-existing.md](tutorials/extend-existing.md) | Étendre une fonctionnalité existante |
| **examples/** | Exemples complets |
| - [examples/chat-server.md](examples/chat-server.md) | Serveur de chat simple |
| - [examples/encrypted-communication.md](examples/encrypted-communication.md) | Communication chiffrée |
| - [examples/periodic-broadcast.md](examples/periodic-broadcast.md) | Diffusion périodique |
| [**contributing.md**](contributing.md) | Guide de contribution |
| [**conventions.md**](conventions.md) | Conventions de développement |
| [**troubleshooting.md**](troubleshooting.md) | FAQ et dépannage |

## Public visé

Cette documentation s'adresse aux développeurs C++ souhaitant :
- Utiliser servd pour construire un serveur M2M performant
- Comprendre le fonctionnement interne du framework
- Étendre ou personnaliser ses fonctionnalités
- Contribuer au projet

## Prérequis

- Connaissances de base en C++ moderne (C++17/20)
- Notions de programmation réseau (TCP/UDP)
- Linux kernel ≥ 5.1 (pour `io_uring`)
- Compilateur GCC ≥ 11 ou Clang ≥ 14
