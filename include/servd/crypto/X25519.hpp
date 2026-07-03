#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>
#include <memory>
#include <botan/x25519.h>
#include <botan/pubkey.h>
#include <botan/auto_rng.h>

namespace servd {

    class X25519 {
    public:
        static constexpr size_t KEY_SIZE = 32;
        using Key = std::array<uint8_t, KEY_SIZE>;

        static Key generate_private() {
            auto rng = std::make_unique<Botan::AutoSeeded_RNG>();
            Botan::X25519_PrivateKey privkey(*rng);
            auto span = privkey.raw_private_key_bits();
            Key key{};
            std::memcpy(key.data(), span.data(), std::min(span.size(), key.size()));
            return key;
        }

        static Key public_key(const Key& private_key) {
            Botan::X25519_PrivateKey privkey(
                std::span<const uint8_t>(private_key.data(), private_key.size()));
            auto pub = privkey.public_value();
            Key key{};
            std::memcpy(key.data(), pub.data(), std::min(pub.size(), key.size()));
            return key;
        }

        static Key shared_secret(const Key& private_key, const Key& public_key) {
            auto rng = std::make_unique<Botan::AutoSeeded_RNG>();
            Botan::X25519_PrivateKey privkey(
                std::span<const uint8_t>(private_key.data(), private_key.size()));
            Botan::PK_Key_Agreement ka(privkey, *rng, "X25519", "");
            auto derived = ka.derive_key(32,
                std::span<const uint8_t>(public_key.data(), public_key.size()), "");
            auto raw = derived.bits_of();
            Key key{};
            std::memcpy(key.data(), raw.data(), std::min(raw.size(), key.size()));
            return key;
        }
    };

}
