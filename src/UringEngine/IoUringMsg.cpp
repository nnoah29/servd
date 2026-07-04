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

namespace servd
{

    Task<size_t> Server::UringEngine::async_recvmsg(
        int fd, std::span<std::byte> buffer, struct sockaddr_storage& addr)
    {
        UringOperation op;
        struct iovec iov{buffer.data(), buffer.size()};
        struct msghdr msg{};

        msg.msg_name = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

        io_uring_prep_recvmsg(sqe, fd, &msg, 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);

        co_return static_cast<size_t>(co_await op);
    }

    Task<int> Server::UringEngine::async_sendto(
        int fd, std::span<const std::byte> buffer, const struct sockaddr_storage& addr)
    {
        UringOperation op;
        struct iovec iov{const_cast<std::byte*>(buffer.data()), buffer.size()};
        struct msghdr msg{};

        msg.msg_name = const_cast<struct sockaddr_storage*>(&addr);
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

        io_uring_prep_sendmsg(sqe, fd, &msg, 0);
        io_uring_sqe_set_data(sqe, &op);
        io_uring_submit(&ring);

        co_return co_await op;
    }

}
