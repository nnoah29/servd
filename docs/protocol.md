# Protocole filaire

## Mode binaire (recommandé)

### Header (16 bytes, little-endian)

```
Offset  Taille  Champ
0       2       command_id       (uint16_t)
2       2       flags            (uint16_t)  — réservé
4       4       payload_length   (uint32_t)
8       8       session_id       (uint64_t)
```

### Trame complète

```
[FrameHeader (16)] [Payload (payload_length bytes)]
```

## Mode texte

```
<command_id|name> <flags> <session_id>
<payload>
```

Deux lignes : header puis payload. Lecture ~10× plus lente que le binaire.

## Commandes réservées

| ID | Nom | Usage |
|---|---|---|
| `0x00F0` | `CMD_KEY_EXCHANGE` | Échange de clés X25519 |
| `0x00F1` | `CMD_ENCRYPTED_MESSAGE` | Message chiffré AES-GCM |

## Réponse

La réponse est une trame avec le même `command_id` que la requête. Structure :

```
[FrameHeader (16)] [ResponsePayload]
```

## Découverte (UDP broadcast)

### Paquet (9 bytes)

```
Offset  Taille  Champ
0       4       magic_number     (uint32_t)
4       1       action           (uint8_t: 0x01=requête client, 0x02=annonce serveur)
5       4       tcp_port         (int32_t)
9       4       udp_port         (int32_t)
```

## Exemple : construction d'une trame binaire (Python)

```python
import socket, struct

header = struct.pack('<HHIQ', command_id, flags, len(payload), session_id)
sock.sendall(header + payload)
```
