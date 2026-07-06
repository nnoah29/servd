# Protocole filaire

servd définit un protocole applicatif binaire (ou texte) par-dessus TCP/UDP/Unix sockets.

## Header

```cpp
#include <servd/Protocol.hpp>
```

## Modes de protocole

### Mode binaire (par défaut)

**FrameHeader** — 16 bytes, format little-endian :

```
Offset  Size  Champ
0       2     command_id    (uint16_t)
2       2     flags         (uint16_t)  — réservé pour usage futur
4       4     payload_length (uint32_t)
8       8     session_id    (uint64_t)
```

```cpp
struct FrameHeader {
    uint16_t command_id;
    uint16_t flags;
    uint32_t payload_length;
    uint64_t session_id;
} __attribute__((packed));
```

La trame complète est : `[FrameHeader (16 bytes)] [Payload (payload_length bytes)]`

### Mode texte

Format texte ligne par ligne :

```
<command_id> <flags> <session_id>
<payload>
```

Où `command_id` peut être un nombre ou un nom (si enregistré via `add_command_name`).

Le mode texte est environ 10× plus lent (lecture octet par octet pour détecter les fins de ligne). Il est déconseillé en production.

## Commandes réservées

```cpp
constexpr uint16_t CMD_KEY_EXCHANGE     = 0x00F0;
constexpr uint16_t CMD_ENCRYPTED_MESSAGE = 0x00F1;
```

Ces IDs (0x00F0–0x00FF) sont réservés par le framework et ne peuvent pas être utilisés par l'application.

## Réponse

```cpp
struct ResponseFrame {
    uint16_t flags;
    std::vector<std::byte> payload;
};
```

Le `command_id` de la réponse est implicitement le même que celui de la requête.
Le `session_id` n'est pas renvoyé (le transport sous-jacent assure l'association).

## Trames spéciales

### Key Exchange (0x00F0)

```
Client → Serveur : payload = [clé publique X25519 (32 bytes)]
Serveur → Client : payload = [clé publique serveur (32 bytes)]
```

Après cet échange, les deux parties partagent une clé AES-256-GCM dérivée du `shared_secret` X25519 via une dérivation simple (les 32 premiers bytes du shared_secret sont utilisés comme clé AES).

### Message chiffré (0x00F1)

```
Client → Serveur : payload = [IV (12)] [Tag (16)] [ciphertext]
                  où ciphertext = AES-GCM( inner_cmd(2) + inner_payload )
Serveur → Client : payload = [IV (12)] [Tag (16)] [ciphertext]
                  où ciphertext = AES-GCM( inner_response_payload )
```

La commande réelle (`inner_cmd`) est encapsulée. Le framework déchiffre, route, puis rechiffre la réponse automatiquement.

## Découverte réseau

### Paquet de découverte (DiscoveryPacket)

```cpp
struct DiscoveryPacket {
    uint32_t magic_number;       // Identifiant magic du réseau
    DiscoveryAction action;      // CLIENT_LOOKING_FOR_SERVER ou SERVER_ANNOUNCING
    int32_t tcp_port;            // Port TCP du serveur (ou 0)
    int32_t udp_port;            // Port UDP du serveur (ou 0)
} __attribute__((packed));
```

### Actions

```cpp
enum class DiscoveryAction : uint8_t {
    CLIENT_LOOKING_FOR_SERVER = 0x01,  // Client cherche un serveur
    SERVER_ANNOUNCING         = 0x02   // Le serveur répond
};
```

### Flux de découverte

```
Client (broadcast UDP port 42069)        Serveur (écoute port 42069)
  │                                            │
  │  DiscoveryPacket{                         │
  │    magic=0xCAFEBABE,                      │
  │    action=CLIENT_LOOKING_FOR_SERVER       │
  │  }                                        │
  │ ──────────────────────────────────────►   │
  │                                            │
  │  DiscoveryPacket{                         │
  │    magic=0xCAFEBABE,                      │
  │    action=SERVER_ANNOUNCING,              │
  │    tcp_port=8080,                         │
  │    udp_port=8081                          │
  │  }                                        │
  │ ◄──────────────────────────────────────   │
```

## TransportType

```cpp
enum class TransportType : uint8_t {
    ANY  = 0,
    TCP  = 1,
    UDP  = 2,
    UNIX = 3
};
```

## ProtocolMode

```cpp
enum class ProtocolMode : uint8_t {
    BINARY = 0,   // Par défaut, recommandé
    TEXT   = 1    // Debug uniquement
};
```

## Exemple : envoi d'une trame binaire (côté client Python)

```python
import socket, struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))

# Commande PING (0x01), session_id=0, flags=0, payload=vide
header = struct.pack('<HHIQ', 0x01, 0, 0, 0)
sock.sendall(header)

# Lecture de la réponse
resp_header = sock.recv(16)
cmd, flags, length, sid = struct.unpack('<HHIQ', resp_header)
payload = sock.recv(length) if length > 0 else b''
print(f"Réponse: cmd={cmd}, payload={payload}")
```
