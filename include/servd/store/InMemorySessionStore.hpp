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
#include <servd/interfaces/ISessionStore.hpp>
#include <unordered_map>

namespace servd {

    class InMemorySessionStore : public ISessionStore {
    public:
        InMemorySessionStore() = default;
        ~InMemorySessionStore() override = default;

        Task<Session> get_or_create(uint64_t session_id) override {
            auto it = store_.find(session_id);
            if (it != store_.end()) {
                co_return it->second;
            }
            Session new_session(session_id);
            store_.insert({session_id, new_session});
            co_return new_session;
        }

        Task<void> save(const Session& session) override {
            store_.insert_or_assign(session.id(), session);
            co_return;
        }

    private:
        std::unordered_map<uint64_t, Session> store_;
    };

}
