# Chiffrement

servd intègre un chiffrement de bout en bout optionnel utilisant **X25519** pour l'échange de clés et **AES-256-GCM** pour le chiffrement symétrique.

La bibliothèque **Botan 3** est utilisée comme backend cryptographique.

## Headers

```cpp
#include <servd/crypto/X25519.hpp>
#include <servd/crypto/AesGcm.hpp>
#include <servd/crypto/Rng.hpp>
```

## X25519 — Échange de clés

```cpp
namespace servd {
    class X25519 {
    public:
        static constexpr size_t KEY_SIZE = 32;
        using Key = std::array<uint8_t, 32>;

        static Key generate_private();           // CSPRNG → clé privée
        static Key public_key(const Key& priv);  // Clé privée → clé publique
        static Key shared_secret(const Key& priv, const Key& pub);
    };
}
```

### Flux complet (côté serveur)

```cpp
// 1. Client envoie sa clé publique X25519 (32 bytes)
// 2. Serveur génère son keypair
auto server_priv = X25519::generate_private();
auto server_pub = X25519::public_key(server_priv);
auto shared = X25519::shared_secret(server_priv, client_pub);

// 3. La clé AES est dérivée du shared_secret
session.set_aes_key(shared);

// 4. Le serveur renvoie sa clé publique
// → Le client peut calculer le même shared_secret
```

## AES-256-GCM — Chiffrement symétrique

```cpp
namespace servd {
    class AesGcm {
    public:
        static constexpr size_t KEY_SIZE   = 32;   // AES-256
        static constexpr size_t NONCE_SIZE = 12;   // IV
        static constexpr size_t TAG_SIZE   = 16;   // Tag d'authentification

        AesGcm(const std::array<uint8_t, KEY_SIZE>& key);

        // Chiffrement
        std::vector<uint8_t> encrypt(
            std::span<const uint8_t> plaintext,
            std::span<const uint8_t> aad,
            std::span<const uint8_t> nonce
        );  // retourne ciphertext + tag (concatenated)

        // Déchiffrement
        std::vector<uint8_t> decrypt(
            std::span<const uint8_t> ciphertext,  // inclut le tag (TAG_SIZE bytes)
            std::span<const uint8_t> aad,
            std::span<const uint8_t> nonce
        );
    };
}
```

### Format des messages chiffrés

Sur le wire, le payload d'un `CMD_ENCRYPTED_MESSAGE` est :

```
[Nonce/IV (12 bytes)] [Ciphertext (N bytes)] [Tag (16 bytes)]
```

Le ciphertext contient la commande interne (2 bytes, uint16_t little-endian) suivie du payload interne.

## RNG — Générateur aléatoire

```cpp
namespace servd {
    class Rng {
    public:
        static void fill(std::span<uint8_t> buf);
          // getrandom() avec GRND_NONBLOCK
        template <size_t N>
        static std::array<uint8_t, N> gen();
    };
}
```

Utilisé pour :
- Générer les clés privées X25519
- Générer les IV/nonces AES-GCM

Basé sur l'appel système `getrandom()` — pas besoin de seed, pas de pool, pas de thread-local.

## Workflow complet d'un message chiffré

```
CLIENT                              SERVER
  │                                    │
  │  [1] Key Exchange (CMD 0x00F0)     │
  │  envoie clé publique X25519 ────►  │ → génère keypair
  │                                    │ → shared_secret → AES key
  │    ◄──── reçoit clé publique       │
  │  → shared_secret → AES key         │
  │                                    │
  │  [2] Message chiffré (CMD 0x00F1) │
  │  encrypt(inner_cmd + payload)      │
  │  envoie IV + ciphertext + tag ──►  │ → decrypt(IV + ciphertext + tag)
  │                                    │ → extract inner_cmd
  │                                    │ → route inner_cmd
  │                                    │ → encrypt(response)
  │    ◄──── IV + ciphertext + tag     │
  │  → decrypt(response)               │
```

## Client Python de test

Voir `examples/test_encryption.py` pour une implémentation complète côté client.
