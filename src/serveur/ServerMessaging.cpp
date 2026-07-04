#include "detail/Engine.hpp"

namespace servd
{

    Task<void> Server::send_to(uint64_t session_id, uint16_t command_id, bytes payload) const
    {
        return engine_->send_to_session(session_id, command_id, payload);
    }

    Task<void> Server::broadcast(uint16_t command_id, bytes payload) const
    {
        return engine_->do_broadcast(command_id, payload);
    }

    Task<void> Server::broadcast_if(uint16_t command_id, bytes payload, std::function<bool(const Session&)> predicate) const {
        return engine_->do_broadcast_if(command_id, payload, std::move(predicate));
    }

}
