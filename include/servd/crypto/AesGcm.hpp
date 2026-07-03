#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if_alg.h>

#ifndef SOL_ALG
#define SOL_ALG 279
#endif

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

            tfm_fd_ = socket(AF_ALG, SOCK_SEQPACKET, 0);
            if (tfm_fd_ < 0) {
                throw std::runtime_error("AesGcm: socket(AF_ALG) failed");
            }

            struct sockaddr_alg sa = {};
            sa.salg_family = AF_ALG;
            std::strncpy(reinterpret_cast<char*>(sa.salg_type), "aead", sizeof(sa.salg_type));
            std::strncpy(reinterpret_cast<char*>(sa.salg_name), "gcm(aes)", sizeof(sa.salg_name));

            if (bind(tfm_fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
                close(tfm_fd_);
                throw std::runtime_error("AesGcm: bind(AF_ALG, gcm(aes)) failed");
            }

            if (setsockopt(tfm_fd_, SOL_ALG, ALG_SET_KEY, key, key_size) < 0) {
                close(tfm_fd_);
                throw std::runtime_error("AesGcm: setsockopt(ALG_SET_KEY) failed");
            }

            op_fd_ = accept(tfm_fd_, nullptr, nullptr);
            if (op_fd_ < 0) {
                close(tfm_fd_);
                throw std::runtime_error("AesGcm: accept(AF_ALG) failed");
            }
        }

        ~AesGcm() {
            if (op_fd_ >= 0) close(op_fd_);
            if (tfm_fd_ >= 0) close(tfm_fd_);
        }

        AesGcm(AesGcm&& other) noexcept
            : tfm_fd_(other.tfm_fd_), op_fd_(other.op_fd_)
        {
            other.tfm_fd_ = -1;
            other.op_fd_ = -1;
        }

        AesGcm& operator=(AesGcm&& other) noexcept {
            if (this != &other) {
                if (op_fd_ >= 0) close(op_fd_);
                if (tfm_fd_ >= 0) close(tfm_fd_);
                tfm_fd_ = other.tfm_fd_;
                op_fd_ = other.op_fd_;
                other.tfm_fd_ = -1;
                other.op_fd_ = -1;
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

            // Output = plaintext + tag
            std::vector<std::byte> out(plaintext.size() + TAG_SIZE);
            size_t out_len = out.size();

            // Prepare request
            struct cmsghdr* cmsg;
            union {
                struct af_alg_iv iv;
                char buf[CMSG_SPACE(sizeof(struct af_alg_iv) + NONCE_SIZE)];
            } cmsg_buf = {};

            struct msghdr msg = {};
            struct iovec iov[2];

            msg.msg_control = cmsg_buf.buf;
            msg.msg_controllen = sizeof(cmsg_buf.buf);

            cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level = SOL_ALG;
            cmsg->cmsg_type = ALG_SET_IV;
            cmsg->cmsg_len = CMSG_LEN(sizeof(struct af_alg_iv) + NONCE_SIZE);
            std::memcpy(CMSG_DATA(cmsg), nonce.data(), NONCE_SIZE);

            // AAD
            iov[0].iov_base = const_cast<std::byte*>(aad.data());
            iov[0].iov_len = aad.size();

            // Plaintext
            iov[1].iov_base = const_cast<std::byte*>(plaintext.data());
            iov[1].iov_len = plaintext.size();

            msg.msg_iov = iov;
            msg.msg_iovlen = 2;

            // Set output size hint
            if (setsockopt(op_fd_, SOL_ALG, ALG_SET_AEAD_AUTHSIZE, nullptr, 0) < 0) {
                throw std::runtime_error("AesGcm: setsockopt(ALG_SET_AEAD_AUTHSIZE) failed");
            }

            // Send plaintext + aad
            if (sendmsg(op_fd_, &msg, 0) < 0) {
                throw std::runtime_error("AesGcm: sendmsg(encrypt) failed");
            }

            // Read ciphertext + tag
            ssize_t n = read(op_fd_, out.data(), out_len);
            if (n < 0 || static_cast<size_t>(n) != out_len) {
                throw std::runtime_error("AesGcm: read(encrypt) failed");
            }

            return out;
        }

        std::vector<std::byte> decrypt(
            std::span<const std::byte> ciphertext,
            std::span<const std::byte> aad,
            std::span<const std::byte> nonce)
        {
            if (nonce.size() != NONCE_SIZE) {
                throw std::runtime_error("AesGcm: nonce must be 12 bytes");
            }
            if (ciphertext.size() < TAG_SIZE) {
                throw std::runtime_error("AesGcm: ciphertext too short (no tag)");
            }

            size_t ct_len = ciphertext.size() - TAG_SIZE;
            std::vector<std::byte> out(ct_len);

            union {
                struct af_alg_iv iv;
                char buf[CMSG_SPACE(sizeof(struct af_alg_iv) + NONCE_SIZE)];
            } cmsg_buf = {};

            struct msghdr msg = {};
            struct iovec iov[2];

            msg.msg_control = cmsg_buf.buf;
            msg.msg_controllen = sizeof(cmsg_buf.buf);

            auto* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level = SOL_ALG;
            cmsg->cmsg_type = ALG_SET_IV;
            cmsg->cmsg_len = CMSG_LEN(sizeof(struct af_alg_iv) + NONCE_SIZE);
            std::memcpy(CMSG_DATA(cmsg), nonce.data(), NONCE_SIZE);

            iov[0].iov_base = const_cast<std::byte*>(aad.data());
            iov[0].iov_len = aad.size();

            iov[1].iov_base = const_cast<std::byte*>(ciphertext.data());
            iov[1].iov_len = ciphertext.size();

            msg.msg_iov = iov;
            msg.msg_iovlen = 2;

            if (sendmsg(op_fd_, &msg, 0) < 0) {
                throw std::runtime_error("AesGcm: sendmsg(decrypt) failed");
            }

            ssize_t n = read(op_fd_, out.data(), out.size());
            if (n < 0 || static_cast<size_t>(n) != out.size()) {
                throw std::runtime_error("AesGcm: read(decrypt) failed - auth tag mismatch");
            }

            return out;
        }

    private:
        int tfm_fd_ = -1;
        int op_fd_ = -1;
    };

}
