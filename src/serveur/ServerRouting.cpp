#include "detail/Engine.hpp"

namespace servd
{

    Server& Server::add_command_name(const std::string& name, uint16_t command_id) {
        text_command_names_[name] = command_id;
        return *this;
    }

    Endpoint& Server::add_command(uint16_t command_id, Handler handler){
        return router_.add(command_id, std::move(handler));
    }

}
