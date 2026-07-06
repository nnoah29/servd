# Chiffrement

servd intègre un chiffrement de bout en bout optionnel via X25519 (échange de clés) + AES-256-GCM (chiffrement symétrique), basé sur Botan 3.

## Activation

Le chiffrement est **transparent** côté serveur. Dès qu'un client effectue un key exchange, les futures commandes chiffrées sont automatiquement déchiffrées et routées.

## Protocole

1. **Key Exchange** : Le client envoie `CMD_KEY_EXCHANGE` (0x00F0) avec sa clé publique X25519 (32 bytes).
2. **Réponse** : Le serveur génère son keypair, calcule le shared secret, stocke la clé AES dans la session, et renvoie sa clé publique.
3. **Messages chiffrés** : Le client envoie `CMD_ENCRYPTED_MESSAGE` (0x00F1) contenant [IV(12) + ciphertext + tag(16)]. Le serveur déchiffre, route la commande interne, chiffre la réponse et la renvoie.

## Côté client (Python)

```python
from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
import os, struct, hashlib

# 1. Key Exchange
client_priv = x25519.X25519PrivateKey.generate()
client_pub = client_priv.public_key().public_bytes_raw()
# envoyer CMD_KEY_EXCHANGE avec client_pub
# recevoir server_pub_bytes

server_pub = x25519.X25519PublicKey.from_public_bytes(server_pub_bytes)
shared = client_priv.exchange(server_pub)
aes_key = hashlib.sha256(shared).digest()[:32]
aesgcm = AESGCM(aes_key)

# 2. Envoyer une commande chiffrée
def send_encrypted(sock, inner_cmd, inner_payload, session_id):
    plaintext = struct.pack('<H', inner_cmd) + inner_payload
    nonce = os.urandom(12)
    ciphertext = aesgcm.encrypt(nonce, plaintext, None)
    frame = struct.pack('<HHIQ', 0x00F1, 0, len(nonce)+len(ciphertext), session_id)
    sock.sendall(frame + nonce + ciphertext)
```

Voir `examples/test_encryption.py` pour une implémentation complète.

## Notes

- La clé AES est dérivée du shared secret X25519 (SHA-256 des 32 bytes)
- Chaque message utilise un IV aléatoire de 12 bytes
- Le chiffrement n'est **pas obligatoire** — utilisez-le seulement si vous en avez besoin
- Impossible d'utiliser les IDs 0x00F0–0x00FF pour vos commandes (réservés)
