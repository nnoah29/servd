# Structure du projet

```
servd/
├── CMakeLists.txt                    # Build principal (bibliothèque statique + exécutable)
├── LICENSE                           # MIT License
├── README.md                         # Documentation principale en anglais
├── ARCHITECTURE.md                   # Vue architecture (français)
├── TODO.md                           # Roadmap et limitations
│
├── app/
│   └── main.cpp                      # Point d'entrée de l'application exemple
│
├── config/
│   └── servd.conf                    # Exemple de fichier de configuration
│
├── cmake/
│   ├── CPM.cmake                     # Gestionnaire de dépendances CPM (v0.42.3)
│   └── servdConfig.cmake.in          # Template de package CMake pour installation
│
├── include/servd/                    # ★ API publique (headers installables)
│   ├── Server.hpp                    # Classe principale Server
│   ├── Protocol.hpp                  # Définitions du protocole filaire
│   ├── Context.hpp                   # Contexte de requête avec property bag
│   ├── Task.hpp                      # Type coroutine Task<T>, Task<void>, DetachedTask
│   ├── auth/
│   │   └── DefaultAuthenticator.hpp  # Implémentation par défaut de l'authentification
│   ├── crypto/
│   │   ├── AesGcm.hpp                # Chiffrement AES-256-GCM
│   │   ├── Rng.hpp                   # Générateur aléatoire cryptographique
│   │   └── X25519.hpp                # Échange de clés X25519
│   ├── interfaces/
│   │   ├── IAuthenticator.hpp        # Interface d'authentification (Strategy)
│   │   ├── IConnection.hpp           # Interface de connexion transport (Strategy)
│   │   ├── ISessionStore.hpp         # Interface de stockage de sessions (Strategy)
│   │   └── Session.hpp               # Classe Session (données de session)
│   ├── router/
│   │   ├── Endpoint.hpp              # Point de terminaison de route (handler + contraintes)
│   │   └── Router.hpp                # Routeur O(1) à tableau fixe
│   └── store/
│       └── InMemorySessionStore.hpp  # Stockage de sessions en mémoire par défaut
│
├── src/
│   ├── detail/
│   │   └── Engine.hpp                # ★ Moteur interne (non public) : UringEngine,
│   │                                 #   UringOperation, ClientFrame, connexions internes
│   ├── serveur/                      # Implémentations de Server
│   │   ├── ServerConfiguration.cpp   # Constructeur, destructeur, enable_*
│   │   ├── ServerConfigFile.cpp      # Parsing du fichier de configuration
│   │   ├── ServerLifecycle.cpp       # init(), run(), stop()
│   │   ├── ServerMessaging.cpp       # send_to(), broadcast(), broadcast_if()
│   │   └── ServerRouting.cpp         # add_command(), add_command_name()
│   └── UringEngine/                  # ★ Cœur io_uring
│       ├── EngineCore.cpp            # Constructeur, destructeur, run(), register_session
│       ├── IoUring.cpp               # async_accept, async_read, async_write, async_read_exact
│       ├── IoUringMsg.cpp            # async_recvmsg, async_sendto (UDP)
│       ├── FrameIO.cpp               # read_frame, read_text_line, read_text_frame
│       ├── ClientHandlers.cpp        # handle_client, text_handle_client,
│       │                             #   start_accept_loop, start_udp_loop,
│       │                             #   periodic_timer_loop
│       ├── CommandRoute.cpp          # process_command, handle_key_exchange,
│       │                             #   handle_normal_command
│       ├── EncryptedHandler.cpp      # handle_encrypted_message (déchiffrement/routage/chiffrement)
│       ├── ConnectionImpl.cpp        # UringTcpConnection::send_frame,
│       │                             #   TextTcpConnection::send_frame
│       ├── UdpConnection.cpp         # UringUdpConnection::send_frame, get_remote_address
│       └── SessionBroadcast.cpp      # send_to_session, do_broadcast, do_broadcast_if
│
├── Logger/
│   ├── Logger.hpp                    # Logger ANSI-color avec macros LOG/LOGS
│   └── Logger.cpp                    # Implémentation du logger
│
├── examples/
│   ├── client_test.py                # Client Python pour le protocole binaire
│   └── test_encryption.py            # Client Python pour le chiffrement
│
├── doc/                              # ★ Documentation (ce dossier)
│   ├── README.md                     # Index de la documentation
│   ├── ...
│
└── packaging/                        # (réservé pour futur empaquetage)
```

## Rôle de chaque dossier

### `include/servd/` — **API publique**

Contient tous les headers installables. Un projet externe utilisant servd n'inclut que ces fichiers :

```cpp
#include <servd/Server.hpp>
#include <servd/Protocol.hpp>
#include <servd/Context.hpp>
#include <servd/interfaces/IAuthenticator.hpp>
#include <servd/interfaces/ISessionStore.hpp>
```

**Règle** : Tout symbole dans `include/servd/` fait partie de l'API publique stable. La compatibilité ascendante est garantie autant que possible. Tout symbole en dehors (dans `src/`) est interne et peut changer sans préavis.

### `src/` — **Implémentation interne**

- `src/detail/` : Contient `Engine.hpp`, le header interne décrivant `UringEngine` (le PIMPL de `Server`), `UringOperation` (le pont coroutine ↔ io_uring), et les classes de connexion internes. Ce fichier n'est jamais installé.
- `src/serveur/` : Implémente les méthodes de `Server` découpées en unités logiques (configuration, cycle de vie, routage, messagerie).
- `src/UringEngine/` : Implémente le moteur io_uring. Chaque fichier couvre une responsabilité unique (lecture de trames, routage de commandes, handlers clients, etc.).

### `Logger/` — **Système de journalisation**

Module autonome de logging avec support ANSI colors, niveaux (DEBUG, INFO, WARN, ERROR), sortie fichier, et macros avec capture automatique de `__FILE__`/`__LINE__`.

### `app/` — **Point d'entrée applicatif**

Contient `main.cpp` qui sert à la fois d'exemple et de binaire de démonstration.

### `config/` — **Fichiers de configuration**

Exemple de fichier `.env`-style pour configurer le serveur sans code.

### `examples/` — **Clients de test Python**

Scripts Python pour tester le protocole binaire et le chiffrement.

### `cmake/` — **Outils CMake**

- `CPM.cmake` : Gestionnaire de dépendances (permettra d'ajouter des dépendances automatiquement)
- `servdConfig.cmake.in` : Template pour générer le fichier de configuration CMake à l'installation
