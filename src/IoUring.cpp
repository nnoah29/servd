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

#include "detail/Engine.hpp"
#include <Logger.hpp>
#include <stdexcept>

namespace servd
{

    Task<int> Server::UringEngine::async_accept(int server_fd)
    {
        UringOperation op;
        struct sockaddr_storage client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_accept(sqe, server_fd,
            reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len, 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        co_return co_await op;
    }

    Task<int> Server::UringEngine::async_read(int fd, std::span<std::byte> buffer)
    {
        UringOperation op;
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, fd, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        co_return co_await op;
    }

    Task<int> Server::UringEngine::async_write(int fd, std::span<const std::byte> buffer)
    {
        UringOperation op;
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_send(sqe, fd, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);
        co_return co_await op;
    }

    Task<void> Server::UringEngine::async_read_exact(int fd, std::span<std::byte> buffer)
    {
        size_t total_read = 0;
        while (total_read < buffer.size()) {
            const int bytes = co_await async_read(fd, buffer.subspan(total_read));
            if (bytes == 0) {
                throw std::runtime_error("Connexion fermee par le client");
            }
            total_read += bytes;
        }
    }

}
