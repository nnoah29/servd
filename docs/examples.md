# Exemples

## Serveur de chat

```cpp
constexpr uint16_t CMD_AUTH = 0x01;
constexpr uint16_t CMD_MSG  = 0x02;
constexpr uint16_t CMD_BCAST = 0x06;

Server app;
app.enable_tcp(8080)
   .add_command(CMD_AUTH, [](Context& ctx) -> Task<ResponseFrame> {
        std::string name((char*)ctx.payload().data(), ctx.payload().size());
        ctx.session().set_authenticated(true, name);
        LOG(INFO, "%s a rejoint le chat", name.c_str());
        return { { 0, encode("Bienvenue " + name) } };
   })
   .add_command(CMD_MSG, [](Context& ctx) -> Task<ResponseFrame> {
        std::string msg((char*)ctx.payload().data(), ctx.payload().size());
        std::string full = std::string(ctx.session().user_identifier()) + ": " + msg;
        // Diffuser à tous sauf à l'expéditeur
        co_await broadcast_if(CMD_BCAST, encode(full),
            [sid = ctx.header().session_id](const Session& s) {
                return s.is_authenticated() && s.id() != sid;
            });
        return { { 0, {} } };
   });
```

## Diffusion périodique

```cpp
app.add_periodic_task(std::chrono::seconds(30), [](Server& srv) -> Task<void> {
    co_await srv.broadcast_if(CMD_ALERT, encode("Rappel: sauvegardez vos données"),
        [](const Session& s) { return s.is_authenticated(); });
});
```

## Client Python (binaire)

```python
import socket, struct, threading, time

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))

def listen():
    while True:
        hdr = sock.recv(16)
        if not hdr: break
        cmd, fl, ln, sid = struct.unpack('<HHIQ', hdr)
        payload = sock.recv(ln) if ln else b''
        print(f"[{cmd}] {payload.decode()}")

threading.Thread(target=listen, daemon=True).start()

# Envoyer PING
sock.sendall(struct.pack('<HHIQ', 0x01, 0, 0, 0))
time.sleep(1)
# Envoyer LOGIN
payload = b"admin"
sock.sendall(struct.pack('<HHIQ', 0x02, 0, len(payload), 0x01) + payload)
time.sleep(60)
```

Voir aussi :
- `examples/client_test.py` — client de test complet
- `examples/test_encryption.py` — client avec chiffrement X25519 + AES-GCM
