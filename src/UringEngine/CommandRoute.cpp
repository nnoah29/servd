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

#include "../detail/Engine.hpp"
#include <servd/Logger.hpp>
#include <cstring>
#include <servd/Protocol.hpp>
#include <servd/Context.hpp>
#include <servd/interfaces/Session.hpp>
#include <servd/crypto/X25519.hpp>
#include <servd/crypto/AesGcm.hpp>

namespace servd
{
    Task<void> Server::UringEngine::handle_key_exchange(
        const ClientFrame& frame, IConnection& connection, Session& session)
    {
        SERVD_LOG(Logger::LogLevel::INFO, "[KeyExchange] Session %lu: starting X25519 handshake", session.id());

        if (frame.payload.size() != 32) {
            SERVD_LOG(Logger::LogLevel::WARN, "[KeyExchange] Invalid key size: %zu", frame.payload.size());
            co_return;
        }

        X25519::Key client_pub{};
        std::memcpy(client_pub.data(), frame.payload.data(), 32);

        const X25519::Key server_priv = X25519::generate_private();
        const X25519::Key server_pub = X25519::public_key(server_priv);
        const X25519::Key shared = X25519::shared_secret(server_priv, client_pub);

        session.set_aes_key(shared);

        SERVD_LOG(Logger::LogLevel::INFO, "[KeyExchange] Session %lu: X25519 shared secret established.", session.id());

        co_await connection.send_frame({CMD_KEY_EXCHANGE, 0, 32, session.id()},
            {reinterpret_cast<const std::byte*>(server_pub.data()), 32});
    }

    Task<void> Server::UringEngine::handle_normal_command(
        const ClientFrame& frame, IConnection& connection, Session& session) const
    {
        SERVD_LOG(Logger::LogLevel::DEBUG, "[Command] Processing CMD %u from session %lu", frame.header.command_id, session.id());

        const auto endpoint = server_.router_.get(frame.header.command_id);
        if (!endpoint) {
            SERVD_LOG(Logger::LogLevel::WARN, "[Reject] Unknown command: %u", frame.header.command_id);
            co_return;
        }
        Context ctx(frame.header, frame.payload, session, connection);

        const bool ok = (!endpoint->requires_auth || co_await server_.authenticator_->authenticate(ctx))
               && (endpoint->allowed_transport == TransportType::ANY
                || endpoint->allowed_transport == connection.transport_type());
        if (!ok) {
            SERVD_LOG(Logger::LogLevel::WARN, "[Reject] Invalid security/transport for CMD %u", frame.header.command_id);
            co_return;
        }

        auto [flags, payload] = co_await endpoint->handler(ctx);

        co_await connection.send_frame(
            {frame.header.command_id, flags, static_cast<uint32_t>(payload.size()), session.id()}, payload);
    }

    Task<void> Server::UringEngine::process_command(
        const ClientFrame& frame, IConnection& connection, Session& session) const
    {
        if (frame.header.command_id == CMD_KEY_EXCHANGE) {
            co_await handle_key_exchange(frame, connection, session);
        } else if (frame.header.command_id == CMD_ENCRYPTED_MESSAGE) {
            co_await handle_encrypted_message(frame, connection, session);
        } else {
            co_await handle_normal_command(frame, connection, session);
        }
    }

}
