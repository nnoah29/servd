# Découverte réseau

La découverte réseau permet aux clients de localiser automatiquement le serveur sur le réseau local sans configuration manuelle.

## Principe

Le serveur écoute sur un port UDP de découverte (par exemple 42069). Les clients envoient un paquet broadcast UDP sur ce port. Le serveur répond avec ses informations de connexion (ports TCP et UDP).

## Configuration

```cpp
Server app;

app.enable_discovery({
    .broadcast_port = 42069,         // Port UDP pour la découverte
    .magic_number = 0xCAFEBABE,      // Magic number (identifiant du réseau)
    .respond_to_clients = true,       // Répondre aux requêtes
    .active_announce_if_idle = false  // Annonces actives régulières
});
```

### Optionnel : annonces actives

Si `active_announce_if_idle` est `true`, le serveur envoie périodiquement des annonces broadcast, même sans sollicitation. Utile pour que les clients découvrent le serveur sans envoyer de requête.

## Protocole

### Paquet de découverte

```cpp
struct DiscoveryPacket {
    uint32_t magic_number;       // Identifiant magic du réseau
    DiscoveryAction action;      // Type de paquet
    int32_t tcp_port;            // Port TCP (ou -1 si non disponible)
    int32_t udp_port;            // Port UDP (ou -1 si non disponible)
};
```

### Actions

| Action | Code | Description |
|---|---|---|
| `CLIENT_LOOKING_FOR_SERVER` | 0x01 | Le client cherche un serveur |
| `SERVER_ANNOUNCING` | 0x02 | Le serveur répond |

### Flux

```
CLIENT (broadcast)                        SERVEUR (écoute port 42069 UDP)
    │                                            │
    │  [CLIENT_LOOKING_FOR_SERVER]               │
    │  magic=0xCAFEBABE                          │
    │  action=0x01                               │
    │ ──────────────────────────────────────────►│
    │                                            │
    │  [SERVER_ANNOUNCING]                       │
    │  magic=0xCAFEBABE                          │
    │  action=0x02                               │
    │  tcp_port=8080                             │
    │  udp_port=8081                             │
    │ ◄──────────────────────────────────────────│
    │                                            │
    │  Connexion TCP sur 8080                    │
    │ ──────────────────────────────────────────►│
```

## Implémentation

La découverte n'est pas encore implémentée comme une boucle de réception dédiée dans la version actuelle (v0.1). La structure de configuration et le protocole sont définis ; l'implémentation de l'écoute des paquets de découverte est planifiée pour v0.2.

## Client Python

```python
import socket, struct

# Envoyer une requête de découverte
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

packet = struct.pack('<IBii', 0xCAFEBABE, 0x01, -1, -1)
sock.sendto(packet, ('<broadcast>', 42069))

# Attendre la réponse
sock.settimeout(3.0)
data, addr = sock.recvfrom(1024)
magic, action, tcp_port, udp_port = struct.unpack('<IBii', data)
print(f"Serveur trouvé à {addr[0]}: TCP={tcp_port} UDP={udp_port}")
```

## Notes

- La découverte nécessite `CAP_NET_ADMIN` ou `root` pour le broadcast UDP
- Le `magic_number` permet de filtrer les serveurs sur le réseau (plusieurs applications peuvent coexister)
- Les clients doivent se connecter via le port TCP retourné pour le protocole applicatif
