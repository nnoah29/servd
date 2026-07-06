# Exemple : Diffusion périodique

Cet exemple montre comment utiliser les tâches périodiques pour envoyer des messages à intervalles réguliers.

## Code complet

```cpp
#include <servd/Server.hpp>
#include <Logger/Logger.hpp>
#include <cstring>
#include <format>

using namespace servd;

constexpr uint16_t CMD_STATUS    = 0x01;
constexpr uint16_t CMD_ALERT     = 0x02;
constexpr uint16_t CMD_HEARTBEAT = 0x03;

std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    std::memcpy(v.data(), s.data(), s.size());
    return v;
}

Task<ResponseFrame> handle_status(Context& ctx) {
    // Retourne l'état du serveur
    std::string status = "OK";
    co_return ResponseFrame{ { 0, to_bytes(status) } };
}

int main() {
    Logger::setLevel(LogLevel::INFO);

    Server app;
    app.enable_tcp(8080)
       .enable_udp(8081)
       .add_command(CMD_STATUS, handle_status);

    // Tâche 1 : Heartbeat toutes les 5 secondes (broadcast à tous)
    app.add_periodic_task(std::chrono::seconds(5), [](Server& srv) -> Task<void> {
        LOG(DEBUG, "Heartbeat broadcast");
        co_await srv.broadcast(CMD_HEARTBEAT, to_bytes("ping"));
    });

    // Tâche 2 : Alerte conditionnelle toutes les 30 secondes
    app.add_periodic_task(std::chrono::seconds(30), [](Server& srv) -> Task<void> {
        LOG(INFO, "Envoi d'une alerte périodique aux utilisateurs authentifiés");
        co_await srv.broadcast_if(
            CMD_ALERT,
            to_bytes("Rappel : sauvegardez vos données !"),
            [](const Session& s) {
                return s.is_authenticated();
            }
        );
    });

    // Tâche 3 : Envoi ciblé toutes les minutes
    // (exemple : envoi à une session spécifique)
    app.add_periodic_task(std::chrono::minutes(1), [](Server& srv) -> Task<void> {
        LOG(INFO, "Envoi périodique à la session 42");
        co_await srv.send_to(42, CMD_ALERT, to_bytes("Message spécial pour session 42"));
    });

    LOG(INFO, "Serveur avec tâches périodiques démarré");
    app.init();
    app.run();
}
```

## Client Python

```python
import socket, struct, threading, time

def reader(sock):
    while True:
        try:
            data = sock.recv(16)
            if not data:
                break
            cmd, flags, length, sid = struct.unpack('<HHIQ', data)
            payload = sock.recv(length) if length > 0 else b''
            cmd_name = {
                0x01: "STATUS",
                0x02: "ALERT",
                0x03: "HEARTBEAT",
            }.get(cmd, f"CMD_{cmd}")
            print(f"[{cmd_name}] {payload.decode()}")
        except:
            break

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))

threading.Thread(target=reader, args=(sock,), daemon=True).start()

# Envoyer une requête status
header = struct.pack('<HHIQ', 0x01, 0, 0, 0)
sock.sendall(header)

# Rester connecté pour recevoir les broadcasts
time.sleep(120)
sock.close()
```

## Points clés

- Les tâches périodiques sont des coroutines exécutées dans la boucle io_uring
- Elles ne sont pas préemptives — une tâche longue bloque les autres
- `broadcast()` envoie à toutes les sessions connues
- `broadcast_if()` permet un filtrage par prédicat sur la session
- Les tâches utilisent `IORING_OP_TIMEOUT` pour le réveil sans polling
