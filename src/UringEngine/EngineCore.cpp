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
#include <Logger.hpp>
#include <stdexcept>
#include <csignal>

namespace servd
{

    namespace {
        volatile std::sig_atomic_t g_signal_received = 0;

        extern "C" void signal_handler(int sig) {
            (void)sig;
            g_signal_received = 1;
        }
    }

    Server::UringEngine::UringEngine(Server& s) : server_(s) {
        if (io_uring_queue_init(256, &ring, 0) < 0)
            throw std::runtime_error("io_uring_queue_init failed");
    }

    Server::UringEngine::~UringEngine() {
        io_uring_queue_exit(&ring);
    }

    void Server::UringEngine::register_session(uint64_t session_id, int fd) {
        sessions_[session_id] = fd;
    }

    void Server::UringEngine::unregister_session(uint64_t session_id) {
        sessions_.erase(session_id);
    }

    void Server::UringEngine::run() {
        struct sigaction sa_sigpipe{};
        sigemptyset(&sa_sigpipe.sa_mask);
        sa_sigpipe.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sa_sigpipe, nullptr);

        struct sigaction sa{};
        sa.sa_handler = signal_handler;
        sigfillset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        g_signal_received = 0;
        running = true;

        struct io_uring_cqe *cqe;
        while (running && !g_signal_received) {
            const int res = io_uring_wait_cqe(&ring, &cqe);
            if (res < 0) continue;
            auto *op = static_cast<UringOperation*>(io_uring_cqe_get_data(cqe));
            if (op != nullptr) {
                op->cqe_res = cqe->res;
                if (op->coroutine)
                    op->coroutine.resume();
            }
            io_uring_cqe_seen(&ring, cqe);
        }

        running = false;

        if (g_signal_received)
            LOG(Logger::LogLevel::INFO, "[UringEngine] Signal recu, arret du serveur.");
    }

}
