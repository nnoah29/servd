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
#include <coroutine>
#include <exception>
#include <utility>

namespace servd {

    template<typename T = void>
    class Task {
    public:
        struct promise_type {
            T value_{};
            std::exception_ptr exception_;
            std::coroutine_handle<> continuation_;

            Task get_return_object() {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_always initial_suspend() noexcept { return {}; }

            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    if (h.promise().continuation_)
                        return h.promise().continuation_;
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };

            FinalAwaiter final_suspend() noexcept { return {}; }

            void unhandled_exception() {
                exception_ = std::current_exception();
            }

            template<typename U>
            void return_value(U&& value) {
                value_ = std::forward<U>(value);
            }
        };

        explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
        ~Task() { if (handle_) handle_.destroy(); }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) {
            handle_.promise().continuation_ = continuation;
            return handle_;
        }

        T await_resume() {
            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
            return std::move(handle_.promise().value_);
        }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

    template<>
    class Task<void> {
    public:
        struct promise_type {
            std::exception_ptr exception_;
            std::coroutine_handle<> continuation_;

            Task get_return_object() {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_always initial_suspend() noexcept { return {}; }

            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    if (h.promise().continuation_)
                        return h.promise().continuation_;
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };

            FinalAwaiter final_suspend() noexcept { return {}; }

            void unhandled_exception() {
                exception_ = std::current_exception();
            }

            void return_void() {}
        };

        explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
        ~Task() { if (handle_) handle_.destroy(); }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
            handle_.promise().continuation_ = continuation;
            return handle_;
        }

        void await_resume() const {
            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
        }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

    struct DetachedTask {
        struct promise_type {
            DetachedTask get_return_object() { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() {}
        };
    };

}
