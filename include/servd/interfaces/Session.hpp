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

        private:
            uint64_t id_;
            bool authenticated_ = false;
            std::string user_identifier_;
    };

}
