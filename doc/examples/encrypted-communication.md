# Exemple : Communication chiffrée

Cet exemple illustre l'utilisation complète du chiffrement de bout en bout avec X25519 et AES-256-GCM.

## Serveur

```cpp
#include <servd/Server.hpp>
#include <servd/Protocol.hpp>
#include <Logger/Logger.hpp>

using namespace servd;

constexpr uint16_t CMD_SECRET_DATA = 0x10;
constexpr uint16_t CMD_PING       = 0x01;

Task<ResponseFrame> handle_secret_data(Context& ctx) {
    // Cette commande est reçue déchiffrée automatiquement
    // si le client utilise CMD_ENCRYPTED_MESSAGE

    LOG(INFO, "Donnée secrète reçue: %zu bytes", ctx.payload().size());

    // Réponse (sera automatiquement chiffrée par le framework)
    std::string ack = "Donnée reçue et déchiffrée";
    std::vector<std::byte> resp(ack.size());
    std::memcpy(resp.data(), ack.data(), ack.size());
    co_return ResponseFrame{ { 0, std::move(resp) } };
}

Task<ResponseFrame> handle_ping(Context& ctx) {
    co_return ResponseFrame{ { 0, {} } };
}

int main() {
    Logger::setLevel(LogLevel::INFO);

    Server app;
    app.enable_tcp(8080)
       .add_command(CMD_SECRET_DATA, handle_secret_data)
       .add_command(CMD_PING, handle_ping);

    LOG(INFO, "Serveur chiffré prêt");
    app.init();
    app.run();
}
```

## Client Python (avec chiffrement)

```python
import socket, struct, os, hashlib
from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# --- X25519 ---
client_private = x25519.X25519PrivateKey.generate()
client_public = client_private.public_key()

# --- Connexion ---
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))
sock.settimeout(5.0)

def recv_exact(n):
    data = b''
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("Déconnexion")
        data += chunk
    return data

# --- Key Exchange (CMD 0x00F0) ---
client_pub_bytes = client_public.public_key().public_bytes_raw()
header = struct.pack('<HHIQ', 0x00F0, 0, len(client_pub_bytes), 0)
sock.sendall(header + client_pub_bytes)

resp_header = recv_exact(16)
cmd, flags, length, sid = struct.unpack('<HHIQ', resp_header)
server_pub_bytes = recv_exact(length)

# Dérivation de la clé AES
server_public = x255255.X25519PublicKey.from_public_bytes(server_pub_bytes)
shared = client_private.exchange(server_public)
aes_key = hashlib.sha256(shared).digest()[:32]
aesgcm = AESGCM(aes_key)

def send_encrypted(inner_cmd, inner_payload):
    # Chiffrer inner_cmd (2 bytes) + inner_payload
    plaintext = struct.pack('<H', inner_cmd) + inner_payload
    nonce = os.urandom(12)
    ciphertext = aesgcm.encrypt(nonce, plaintext, None)

    # Envoyer CMD_ENCRYPTED_MESSAGE
    encrypted_payload = nonce + ciphertext
    header = struct.pack('<HHIQ', 0x00F1, 0, len(encrypted_payload), sid)
    sock.sendall(header + encrypted_payload)

def recv_decrypted():
    resp_header = recv_exact(16)
    cmd, flags, length, sid = struct.unpack('<HHIQ', resp_header)
    encrypted = recv_exact(length)

    nonce = encrypted[:12]
    ciphertext = encrypted[12:]
    plaintext = aesgcm.decrypt(nonce, ciphertext, None)
    return plaintext

# --- Envoyer une commande secrète ---
data = b"Ceci est un message ultra secret"
send_encrypted(0x10, data)

# --- Recevoir la réponse déchiffrée ---
response = recv_decrypted()
print(f"Réponse déchiffrée: {response.decode()}")

sock.close()
```

## Prérequis

```bash
pip install cryptography
```

## Exécution

```bash
# Terminal 1
./build/servd

# Terminal 2
python3 examples/test_encryption.py
# Ou le script ci-dessus
python3 encrypted_client.py
```

## Notes

- L'échange de clés (X25519) n'a lieu qu'une fois par session
- Chaque message chiffré utilise un IV aléatoire de 12 bytes
- Le tag AES-GCM (16 bytes) est automatiquement vérifié côté serveur
- Le serveur rechiffre automatiquement les réponses
