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
#include <vector>
#include <optional>
#include <servd/Protocol.hpp>
#include <servd/Context.hpp>
#include <servd/interfaces/Session.hpp>
#include <servd/crypto/AesGcm.hpp>
#include <servd/crypto/Rng.hpp>

namespace servd
{
namespace
{

    struct DecryptedMessage {
        AesGcm cipher;
        uint16_t inner_cmd;
        std::vector<std::byte> inner_payload;
    };

    std::optional<DecryptedMessage> do_decrypt(
        const std::vector<std::byte>& payload, Session& session)
    {
        if (!session.has_aes_key() || payload.size() < AesGcm::NONCE_SIZE + AesGcm::TAG_SIZE)
            return std::nullopt;
        auto s = std::span(payload);
        auto iv = s.subspan(0, AesGcm::NONCE_SIZE);
        auto tag = s.subspan(AesGcm::NONCE_SIZE, AesGcm::TAG_SIZE);
        auto ct = s.subspan(AesGcm::NONCE_SIZE + AesGcm::TAG_SIZE);
        std::vector<std::byte> aei(ct.size() + AesGcm::TAG_SIZE);
        std::memcpy(aei.data(), ct.data(), ct.size());
        std::memcpy(aei.data() + ct.size(), tag.data(), AesGcm::TAG_SIZE);
        AesGcm cipher(session.aes_key().data(), AesGcm::KEY_SIZE);
        std::vector<std::byte> pt;
        try { pt = cipher.decrypt(aei, {}, iv); }
        catch (const std::exception& e) { return std::nullopt; }
        if (pt.size() < sizeof(uint16_t)) return std::nullopt;
        uint16_t inner_cmd = 0;
        std::memcpy(&inner_cmd, pt.data(), sizeof(uint16_t));
        std::vector<std::byte> inner_payload(pt.size() - sizeof(uint16_t));
        if (!inner_payload.empty())
            std::memcpy(inner_payload.data(), pt.data() + sizeof(uint16_t), inner_payload.size());
        SERVD_LOG(Logger::LogLevel::DEBUG, "[Crypte] Cmd %u decryptee (%zu octets)", inner_cmd, inner_payload.size());
        return DecryptedMessage{std::move(cipher), inner_cmd, std::move(inner_payload)};
    }

    Task<void> do_route_and_send(Router& router, IAuthenticator& authenticator, IConnection& conn,
        AesGcm& cipher, uint16_t inner_cmd, std::span<const std::byte> inner_payload, Session& session)
    {
        const auto ep = router.get(inner_cmd);

        if (!ep) {
            SERVD_LOG(Logger::LogLevel::WARN, "[Crypte] Cmd inconnue: %u", inner_cmd);
            co_return;
        }

        Context ctx({inner_cmd, 0, static_cast<uint32_t>(inner_payload.size()), session.id()},
            inner_payload, session, conn);

        const bool ok = (!ep->requires_auth || co_await authenticator.authenticate(ctx))
            && (ep->allowed_transport == TransportType::ANY || ep->allowed_transport == conn.transport_type());

        if (!ok) {
            SERVD_LOG(Logger::LogLevel::WARN, "[Crypte] Rejet cmd %u", inner_cmd); co_return;
        }
        auto [flags, resp] = co_await ep->handler(ctx);
        std::vector<std::byte> inner(sizeof(uint16_t) + resp.size());

        std::memcpy(inner.data(), &inner_cmd, sizeof(uint16_t));
        if (!resp.empty())
            std::memcpy(inner.data() + sizeof(uint16_t), resp.data(), resp.size());

        const auto iv_out = Rng::gen<AesGcm::NONCE_SIZE>();
        const auto enc = cipher.encrypt(inner, {},
            {reinterpret_cast<const std::byte*>(iv_out.data()), AesGcm::NONCE_SIZE});
        const size_t cl = enc.size() - AesGcm::TAG_SIZE;

        std::vector<std::byte> out(AesGcm::NONCE_SIZE + AesGcm::TAG_SIZE + cl);
        std::memcpy(out.data(), iv_out.data(), AesGcm::NONCE_SIZE);
        std::memcpy(out.data() + AesGcm::NONCE_SIZE, enc.data() + cl, AesGcm::TAG_SIZE);
        std::memcpy(out.data() + AesGcm::NONCE_SIZE + AesGcm::TAG_SIZE, enc.data(), cl);

        co_await conn.send_frame(
            {CMD_ENCRYPTED_MESSAGE, flags, static_cast<uint32_t>(out.size()), session.id()}, out);
    }

}

    Task<void> Server::UringEngine::handle_encrypted_message(
        const ClientFrame& frame, IConnection& conn, Session& session) const
    {
        auto msg = do_decrypt(frame.payload, session);
        if (!msg) co_return;
        co_await do_route_and_send(server_.router_, *server_.authenticator_, conn,
            msg->cipher, msg->inner_cmd, msg->inner_payload, session);
    }
}
