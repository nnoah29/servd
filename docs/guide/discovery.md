# Découverte réseau

Le protocole de découverte permet aux clients de localiser le serveur sur le réseau local sans configuration manuelle.

## Principe

Le serveur écoute sur un port UDP pendant que les clients envoient des broadcasts. Le serveur répond avec ses informations de connexion (ports TCP/UDP).

## Configuration

```cpp
app.enable_discovery({
    .broadcast_port = 42069,
    .magic_number = 0xCAFEBABE,    // identifiant du réseau
    .respond_to_clients = true,
    .active_announce_if_idle = false
});
```

## Côté client

```python
import socket, struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

# Requête de découverte
packet = struct.pack('<IBii', 0xCAFEBABE, 0x01, -1, -1)
sock.sendto(packet, ('<broadcast>', 42069))

# Réponse
sock.settimeout(3)
data, addr = sock.recvfrom(1024)
magic, action, tcp, udp = struct.unpack('<IBii', data)
print(f"Serveur à {addr[0]}: TCP {tcp} UDP {udp}")
```

## Notes

- Nécessite `CAP_NET_ADMIN` ou `root` pour le broadcast UDP
- Le `magic_number` permet de filtrer les serveurs sur un réseau partagé
