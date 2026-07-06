# Démarrage rapide

## Prérequis

- Linux ≥ 5.1, CMake ≥ 3.16, compilateur C++20 (GCC ≥ 11 / Clang ≥ 14)
- `libbotan-3-dev` et `liburing-dev`

```bash
# Debian/Ubuntu
sudo apt install cmake g++ libbotan-3-dev liburing-dev
```

## Compilation

```bash
git clone <url> servd
cd servd
cmake -B build && cmake --build build
```

Produit : `build/servd` (exécutable) + `build/libservd_core.a` (bibliothèque statique).

## Premier serveur (app/main.cpp)

```cpp
#include <servd/Server.hpp>
#include <Logger/Logger.hpp>

using namespace servd;

constexpr uint16_t CMD_PING = 0x01;

int main() {
    Server app;

    app.enable_tcp(8080)
       .add_command(CMD_PING, [](Context& ctx) -> Task<ResponseFrame> {
            LOG(INFO, "PING reçu via %d", (int)ctx.current_transport());
            return { { 0, ctx.payload() } };
       });

    app.init();
    app.run();
}
```

```bash
cmake -B build && cmake --build build
sudo ./build/servd
```

## Test rapide

```bash
python3 examples/client_test.py
```

Le client envoie `CMD_PING` (0x01), reçoit la réponse, puis `CMD_LOGIN` (0x02) et reste à l'écoute des notifications push.

## Prochaine étape

Lisez les guides par fonctionnalité dans [guide/](guide/commands.md).
