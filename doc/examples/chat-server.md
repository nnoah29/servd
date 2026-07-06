# Exemple : Serveur de chat

Cet exemple implémente un serveur de chat simple où les clients peuvent s'authentifier, envoyer des messages, et recevoir les messages des autres.

## Code complet

```cpp
#include <servd/Server.hpp>
#include <Logger/Logger.hpp>
#include <cstring>
#include <sstream>
#include <unordered_set>

using namespace servd;

constexpr uint16_t CMD_AUTH    = 0x01;
constexpr uint16_t CMD_MSG     = 0x02;
constexpr uint16_t CMD_JOIN    = 0x03;
constexpr uint16_t CMD_LEAVE   = 0x04;
constexpr uint16_t CMD_USERS   = 0x05;
constexpr uint16_t CMD_BROADCAST = 0x06;

// Utilisateurs connectés (ensemble des sessions authentifiées)
std::unordered_set<uint64_t> g_connected_users;
std::mutex g_users_mutex;

std::vector<std::byte> encode_string(const std::string& s) {
    std::vector<std::byte> v(s.size());
    std::memcpy(v.data(), s.data(), s.size());
    return v;
}

// Authentification
Task<ResponseFrame> handle_auth(Context& ctx) {
    auto sid = ctx.header().session_id;
    auto payload = ctx.payload();

    // Format du payload : "username"
    std::string username(reinterpret_cast<const char*>(payload.data()), payload.size());

    ctx.session().set_authenticated(true, username);

    {
        std::lock_guard lock(g_users_mutex);
        g_connected_users.insert(sid);
    }

    LOG(INFO, "%s a rejoint le chat", username.c_str());

    // Notifier les autres utilisateurs
    auto notif = encode_string(username + " a rejoint le chat");
    co_await ctx.push_event(CMD_BROADCAST, notif);

    co_return ResponseFrame{ { 0, encode_string("Connecté en tant que " + username) } };
}

// Envoi de message à tous les autres utilisateurs
Task<ResponseFrame> handle_msg(Context& ctx) {
    if (!ctx.session().is_authenticated()) {
        co_return ResponseFrame{ { 0, encode_string("Authentifiez-vous d'abord") } };
    }

    auto sid = ctx.header().session_id;
    auto username = ctx.session().user_identifier();
    auto payload = ctx.payload();

    std::string message(reinterpret_cast<const char*>(payload.data()), payload.size());
    std::string full_msg = username + ": " + message;
    auto data = encode_string(full_msg);

    LOG(INFO, "[CHAT] %s", full_msg.c_str());

    // Diffuser à tous sauf à l'expéditeur
    co_await broadcast_if(CMD_BROADCAST, data,
        [sid](const Session& s) {
            return s.is_authenticated() && s.id() != sid;
        }
    );

    co_return ResponseFrame{ { 0, encode_string("Message envoyé") } };
}

// Liste des utilisateurs connectés
Task<ResponseFrame> handle_users(Context& ctx) {
    if (!ctx.session().is_authenticated()) {
        co_return ResponseFrame{ { 0, encode_string("Authentifiez-vous d'abord") } };
    }

    // Note : Dans un vrai serveur, il faudrait interroger le session store
    co_return ResponseFrame{ { 0, encode_string("Fonctionnalité à implémenter") } };
}

int main() {
    Logger::setLevel(LogLevel::INFO);

    Server app;

    app.enable_tcp(8080)
       .set_max_clients(50)
       .add_command(CMD_AUTH, handle_auth)
       .add_command(CMD_MSG, handle_msg)
       .add_command(CMD_USERS, handle_users);

    LOG(INFO, "Serveur de chat prêt sur le port 8080");
    app.init();
    app.run();

    return 0;
}
```

## Client Python de test

```python
import socket, struct, threading, time

def reader(sock):
    while True:
        try:
            data = sock.recv(16)
            if not data:
                break
            cmd, flags, length, sid = struct.unpack('<HHIQ', data)
            if length > 0:
                payload = sock.recv(length)
                print(f"[Serveur] {payload.decode()}")
        except:
            break

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))

# Thread de lecture
threading.Thread(target=reader, args=(sock,), daemon=True).start()

# Authentification
nickname = input("Votre pseudo: ")
payload = nickname.encode()
header = struct.pack('<HHIQ', 0x01, 0, len(payload), 0)
sock.sendall(header + payload)
time.sleep(0.1)

# Boucle de chat
while True:
    msg = input()
    if msg == "/quit":
        break
    payload = msg.encode()
    header = struct.pack('<HHIQ', 0x02, 0, len(payload), 0x01)
    sock.sendall(header + payload)
    time.sleep(0.05)

sock.close()
```

## Utilisation

```bash
# Terminal 1 : serveur
./build/servd

# Terminal 2 : client Alice
python3 chat_client.py

# Terminal 3 : client Bob
python3 chat_client.py
```
