# Créer un nouveau projet

Ce guide vous montre comment créer un nouveau projet utilisant servd de zéro.

## Structure recommandée

```
mon-projet/
├── CMakeLists.txt
├── src/
│   └── main.cpp
└── config/
    └── servd.conf
```

## 1. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(mon_projet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Trouver servd (installé ou via CPM)
find_package(servd REQUIRED)

# Ou via add_subdirectory :
# add_subdirectory(path/to/servd)

add_executable(mon_projet src/main.cpp)
target_link_libraries(mon_projet PRIVATE servd::servd_core)
```

## 2. main.cpp — Serveur minimal

```cpp
#include <servd/Server.hpp>
#include <Logger/Logger.hpp>
#include <cstring>
#include <string_view>

using namespace servd;

constexpr uint16_t CMD_HELLO = 0x01;
constexpr uint16_t CMD_ECHO  = 0x02;

Task<ResponseFrame> handle_hello(Context& ctx) {
    LOG(INFO, "HELLO reçu de session %lu", ctx.header().session_id);
    std::string_view msg = "Bienvenue sur mon serveur !";
    std::vector<std::byte> payload(msg.size());
    std::memcpy(payload.data(), msg.data(), msg.size());
    co_return ResponseFrame{ { 0, std::move(payload) } };
}

Task<ResponseFrame> handle_echo(Context& ctx) {
    co_return ResponseFrame{ { 0, {ctx.payload().begin(), ctx.payload().end()} } };
}

int main() {
    Logger::setLevel(LogLevel::INFO);

    Server app;

    app.enable_tcp(8080, ProtocolMode::BINARY)
       .set_max_clients(100)
       .add_command(CMD_HELLO, handle_hello)
       .add_command(CMD_ECHO, handle_echo);

    LOG(INFO, "Initialisation...");
    app.init();

    LOG(INFO, "Serveur prêt sur le port 8080");
    app.run();

    LOG(INFO, "Serveur arrêté");
    return 0;
}
```

## 3. Compilation et exécution

```bash
cd mon-projet
cmake -B build
cmake --build build
./build/mon_projet
```

## 4. Test avec Python

```python
import socket, struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))

# CMD_HELLO = 0x01
header = struct.pack('<HHIQ', 0x01, 0, 0, 0)
sock.sendall(header)

resp = sock.recv(16)
cmd, flags, length, sid = struct.unpack('<HHIQ', resp)
payload = sock.recv(length) if length else b''
print("Message:", payload.decode())

# CMD_ECHO = 0x02
payload = b"Hello World!"
header = struct.pack('<HHIQ', 0x02, 0, len(payload), sid)
sock.sendall(header + payload)

resp = sock.recv(16)
cmd, flags, length, sid = struct.unpack('<HHIQ', resp)
echoed = sock.recv(length) if length else b''
print("Echo:", echoed.decode())

sock.close()
```

## Prochaine étape

Consultez [Ajouter un nouveau module](new-module.md) pour étendre votre projet avec des fonctionnalités réutilisables.
