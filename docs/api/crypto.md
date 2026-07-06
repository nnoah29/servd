# API : Modules cryptographiques

## `X25519` — Échange de clés

**Header** : `#include <servd/crypto/X25519.hpp>`

```cpp
namespace servd {
    class X25519 {
    public:
        static constexpr size_t KEY_SIZE = 32;
        using Key = std::array<uint8_t, 32>;

        static Key generate_private();
        static Key public_key(const Key& private_key);
        static Key shared_secret(const Key& private_key, const Key& public_key);
    };
}
```

### Exemple

```cpp
// Génération d'une paire de clés
auto priv = X25519::generate_private();
auto pub = X25519::public_key(priv);

// Calcul du secret partagé avec la clé publique du pair
auto shared = X25519::shared_secret(priv, other_pub);
```

---

## `AesGcm` — Chiffrement AES-256-GCM

**Header** : `#include <servd/crypto/AesGcm.hpp>`

```cpp
namespace servd {
    class AesGcm {
    public:
        static constexpr size_t KEY_SIZE   = 32;
        static constexpr size_t NONCE_SIZE = 12;
        static constexpr size_t TAG_SIZE   = 16;

        explicit AesGcm(const std::array<uint8_t, KEY_SIZE>& key);

        // Move-only
        AesGcm(AesGcm&&) noexcept;
        AesGcm& operator=(AesGcm&&) noexcept;

        // Chiffrement : retourne ciphertext + tag (concatenated)
        std::vector<uint8_t> encrypt(
            std::span<const uint8_t> plaintext,
            std::span<const uint8_t> aad,
            std::span<const uint8_t> nonce
        );

        // Déchiffrement : ciphertext doit inclure le tag final
        std::vector<uint8_t> decrypt(
            std::span<const uint8_t> ciphertext,
            std::span<const uint8_t> aad,
            std::span<const uint8_t> nonce
        );
    };
}
```

### Exemple

```cpp
// Initialisation
auto key = X25519::shared_secret(server_priv, client_pub);
AesGcm cipher(key);

// Chiffrement
auto iv = Rng::gen<12>();
std::vector<uint8_t> plaintext = { /* ... */ };
std::vector<uint8_t> aad = { /* additional authenticated data */ };
auto encrypted = cipher.encrypt(plaintext, aad, iv);

// Déchiffrement
auto decrypted = cipher.decrypt(encrypted, aad, iv);

// Format wire : [IV (12)] [Ciphertext (N)] [Tag (16)]
std::vector<uint8_t> wire;
wire.insert(wire.end(), iv.begin(), iv.end());
wire.insert(wire.end(), encrypted.begin(), encrypted.end());
```

---

## `Rng` — Générateur aléatoire

**Header** : `#include <servd/crypto/Rng.hpp>`

```cpp
namespace servd {
    class Rng {
    public:
        static void fill(std::span<uint8_t> buf);
        template <size_t N>
        static std::array<uint8_t, N> gen();
    };
}
```

### Exemple

```cpp
// Générer un IV de 12 bytes
auto iv = Rng::gen<12>();

// Remplir un buffer existant
std::array<uint8_t, 32> key;
Rng::fill(key);
```
