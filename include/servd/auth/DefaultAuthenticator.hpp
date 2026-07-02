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
#include "servd/interfaces/IAuthenticator.hpp"
#include "servd/Context.hpp"

namespace servd {

    class DefaultAuthenticator : public IAuthenticator {
    public:
        Task<bool> authenticate(Context& ctx) override {
            co_return ctx.session().is_authenticated();
        }
    };

}
