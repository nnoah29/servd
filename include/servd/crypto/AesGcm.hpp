#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <botan/cipher_mode.h>
#include <botan/auto_rng.h>
#include <botan/secmem.h>

namespace servd {

    class AesGcm {
    public:
        static constexpr size_t KEY_SIZE = 32;    // AES-256
        static constexpr size_t NONCE_SIZE = 12;  // 96-bit IV
        static constexpr size_t TAG_SIZE = 16;    // GCM tag

        AesGcm(const uint8_t* key, size_t key_size) {
            if (key_size != KEY_SIZE) {
                throw std::runtime_error("AesGcm: key must be 32 bytes (AES-256)");
            }

            enc_ = Botan::Cipher_Mode::create("AES-256/GCM", Botan::Cipher_Dir::Encryption);
            dec_ = Botan::Cipher_Mode::create("AES-256/GCM", Botan::Cipher_Dir::Decryption);

            if (!enc_ || !dec_) {
                throw std::runtime_error("AesGcm: Botan AES-256/GCM not available");
            }

            enc_->set_key(key, key_size);
            dec_->set_key(key, key_size);
        }

        AesGcm(AesGcm&& other) noexcept
            : enc_(std::move(other.enc_)), dec_(std::move(other.dec_))
        {}

        AesGcm& operator=(AesGcm&& other) noexcept {
            if (this != &other) {
                enc_ = std::move(other.enc_);
                dec_ = std::move(other.dec_);
            }
            return *this;
        }

        AesGcm(const AesGcm&) = delete;
        AesGcm& operator=(const AesGcm&) = delete;

        std::vector<std::byte> encrypt(
            std::span<const std::byte> plaintext,
            std::span<const std::byte> aad,
            std::span<const std::byte> nonce)
        {
            if (nonce.size() != NONCE_SIZE) {
                throw std::runtime_error("AesGcm: nonce must be 12 bytes");
            }

            auto nonce_u8 = std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size());

            Botan::secure_vector<uint8_t> buf(plaintext.size());
            if (!plaintext.empty()) {
                std::memcpy(buf.data(), plaintext.data(), plaintext.size());
            }

            enc_->start(nonce_u8);

            if (!aad.empty()) {
                Botan::secure_vector<uint8_t> aad_buf(aad.size());
                std::memcpy(aad_buf.data(), aad.data(), aad.size());
                enc_->update(aad_buf);
            }

            enc_->finish(buf);
            // buf now contains: ciphertext || tag

            std::vector<std::byte> out(buf.size());
            std::memcpy(out.data(), buf.data(), buf.size());
            return out;
        }

        std::vector<std::byte> decrypt(
            std::span<const std::byte> ciphertext_with_tag,
            std::span<const std::byte> aad,
            std::span<const std::byte> nonce)
        {
            if (nonce.size() != NONCE_SIZE) {
                throw std::runtime_error("AesGcm: nonce must be 12 bytes");
            }
            if (ciphertext_with_tag.size() < TAG_SIZE) {
                throw std::runtime_error("AesGcm: ciphertext too short (no tag)");
            }

            auto nonce_u8 = std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size());

            size_t ct_len = ciphertext_with_tag.size() - TAG_SIZE;
            Botan::secure_vector<uint8_t> buf(ciphertext_with_tag.size());
            std::memcpy(buf.data(), ciphertext_with_tag.data(), ciphertext_with_tag.size());

            dec_->start(nonce_u8);

            if (!aad.empty()) {
                Botan::secure_vector<uint8_t> aad_buf(aad.size());
                std::memcpy(aad_buf.data(), aad.data(), aad.size());
                dec_->update(aad_buf);
            }

            dec_->finish(buf);
            // buf now contains plaintext (size = ct_len), throws if tag invalid

            std::vector<std::byte> out(ct_len);
            std::memcpy(out.data(), buf.data(), ct_len);
            return out;
        }

    private:
        std::unique_ptr<Botan::Cipher_Mode> enc_;
        std::unique_ptr<Botan::Cipher_Mode> dec_;
    };

}
