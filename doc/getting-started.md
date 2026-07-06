# Guide de démarrage rapide

## Prérequis système

- **Linux** kernel ≥ 5.1 (nécessaire pour `io_uring`)
- **CMake** ≥ 3.16
- **Compilateur** C++20 : GCC ≥ 11 ou Clang ≥ 14
- **Botan 3** (bibliothèque cryptographique)
- **liburing** (API io_uring)

### Installation des dépendances (Debian/Ubuntu)

```bash
sudo apt install cmake g++ libbotan-3-dev liburing-dev
```

### Installation des dépendances (Arch Linux)

```bash
sudo pacman -S cmake gcc botan liburing
```

### Installation des dépendances (Fedora)

```bash
sudo dnf install cmake gcc-c++ botan-devel liburing-devel
```

## Compilation du framework

```bash
git clone <url-du-depot> servd
cd servd
cmake -B build
cmake --build build
```

Le binaire `servd` est produit dans `build/`. La bibliothèque statique `libservd_core.a` également.

## Premier serveur

Voici le serveur le plus simple possible (extrait de `app/main.cpp`) :

```cpp
#include <servd/Server.hpp>
#include <iostream>

using namespace servd;

constexpr uint16_t CMD_PING   = 0x01;
constexpr uint16_t CMD_LOGIN  = 0x02;
constexpr uint16_t CMD_ALERT  = 0x99;

int main() {
    Server app;

    app.enable_tcp(8080, ProtocolMode::BINARY);
    app.enable_udp(8081);

    app.add_command(CMD_PING, [](Context& ctx) -> Task<ResponseFrame> {
        LOG(INFO, "PING reçu via %d", (int)ctx.current_transport());
        return { { 0, ctx.payload() } };
    });

    app.add_command(CMD_LOGIN, [](Context& ctx) -> Task<ResponseFrame> {
        ctx.session().set_authenticated(true, "user");
        return { { 0, {} } };
    }) .require_auth();  // cette commande nécessite d'être authentifié

    app.add_periodic_task(std::chrono::seconds(10), [](Server& srv) -> Task<void> {
        LOG(INFO, "Broadcast périodique");
        co_await srv.broadcast_if(
            CMD_ALERT,
            std::vector<std::byte>{std::byte('O'), std::byte('K')},
            [](const Session& s) { return s.is_authenticated(); }
        );
    });

    app.init();
    app.run();
}
```

Compilez et exécutez :

```bash
cmake -B build && cmake --build build
sudo ./build/servd   # root nécessaire pour SO_REUSEPORT et discovery
```

## Tester avec un client Python

Un client de test est fourni dans `examples/client_test.py` :

```bash
python3 examples/client_test.py
```

Ce client :
1. Envoie une commande `PING` (0x01)
2. Affiche la réponse (l'ID du transport utilisé)
3. Envoie une commande `LOGIN` (0x02)
4. Reste à l'écoute des notifications push pendant 12 secondes

## Prochaine étape

Poursuivez avec la [vue d'architecture](architecture.md) pour comprendre comment servd fonctionne en profondeur.
