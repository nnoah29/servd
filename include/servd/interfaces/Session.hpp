/*
**  _                                              _      ___    ___
** | |                                            | |    |__ \  / _ \
** | |_Created _       _ __   _ __    ___    __ _ | |__     ) || (_) |
** | '_ \ | | | |     | '_ \ | '_ \  / _ \  / _` || '_ \   / /  \__, |
** | |_) || |_| |     | | | || | | || (_) || (_| || | | | / /_    / /
** |_.__/  \__, |     |_| |_||_| |_| \___/  \__,_||_| |_||____|  /_/
**          __/ |     on 30/06/2026.
**         |___/
*/

#pragma once
#include <string>
#include <cstdint>
#include <array>
#include <cstring>

namespace servd {

        class Session {
        public:
            Session() : id_(0) {}
            explicit Session(uint64_t id) : id_(id) {}

            [[nodiscard]] uint64_t id() const { return id_; }
            [[nodiscard]] bool is_authenticated() const { return authenticated_; }
            void set_authenticated(bool state, std::string user_identifier) {
                authenticated_ = state;
                user_identifier_ = std::move(user_identifier);
            }
            [[nodiscard]] const std::string& user_identifier() const { return user_identifier_; }

        [[nodiscard]] bool has_aes_key() const { return has_aes_key_; }
            void set_aes_key(const std::array<uint8_t, 32>& key) {
                aes_key_ = key;
                has_aes_key_ = true;
            }
            [[nodiscard]] const std::array<uint8_t, 32>& aes_key() const { return aes_key_; }

        private:
            uint64_t id_;
            bool authenticated_ = false;
            bool has_aes_key_ = false;
            std::array<uint8_t, 32> aes_key_{};
            std::string user_identifier_;
    };

}
