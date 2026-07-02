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
#include "Protocol.hpp"
#include "interfaces/IConnection.hpp"
#include "interfaces/Session.hpp"
#include "Task.hpp"

#include <unordered_map>
#include <string>
#include <any>
#include <stdexcept>

namespace servd {

    using bytes = std::span<const std::byte>;

    class Context {
        public:
            Context(FrameHeader header, bytes payload, Session& session, IConnection& connection)
                : session_(session), connection_(connection), header_(header), payload_(payload) {}

            [[nodiscard]] const FrameHeader& header() const { return header_; }
            [[nodiscard]] bytes payload() const { return payload_; }

            [[nodiscard]] Session& session() const { return session_; }
            [[nodiscard]] TransportType current_transport() const{ return connection_.transport_type();}

            [[nodiscard]] Task<void> push_event(uint16_t event_command_id, bytes data) const {
                const FrameHeader header {event_command_id, 0, static_cast<uint32_t>(data.size()), session_.id()};
                return connection_.send_frame(header, data);
            }

            // ========================================================
            // EXTENSIBILITE: Property Bag (std::any)
            // ========================================================

            template<typename T>
            void set(const std::string& key, T&& value) {
                locals_[key] = std::forward<T>(value);
            }

            template<typename T>
            [[nodiscard]] T get(const std::string& key) const {
                return std::any_cast<T>(locals_.at(key));
            }

            template<typename T>
            [[nodiscard]] const T* get_if(const std::string& key) const {
                auto it = locals_.find(key);
                if (it != locals_.end()) {
                    return std::any_cast<T>(&it->second);
                }
                return nullptr;
            }

        private:
            Session& session_;
            IConnection& connection_;
            const FrameHeader header_;
            std::span<const std::byte> payload_;
            std::unordered_map<std::string, std::any> locals_;
    };
}
