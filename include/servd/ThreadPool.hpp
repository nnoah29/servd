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
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>

#include "servd/Task.hpp"

namespace servd {

class ThreadPool {
public:
    using PostCallback = std::function<void(std::coroutine_handle<>)>;

    explicit ThreadPool(size_t thread_count, PostCallback post_cb = nullptr)
        : stop_(false), post_cb_(std::move(post_cb))
    {
        workers_.reserve(thread_count);
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable())
                t.join();
        }
    }

    template<typename F>
    Task<std::invoke_result_t<F>> enqueue(F&& f) {
        using R = std::invoke_result_t<F>;
        static_assert(!std::is_void_v<R>,
            "ThreadPool::enqueue does not support void return type. "
            "Wrap your call in a lambda that returns a value.");

        struct Shared {
            std::mutex mtx;
            std::optional<R> result;
            std::exception_ptr error;
            std::coroutine_handle<> waiter;
            bool done = false;
        };

        auto shared = std::make_shared<Shared>();

        {
            std::unique_lock lock(mutex_);
            tasks_.push([shared, f = std::forward<F>(f), post_cb = post_cb_]() {
                R value;
                try {
                    value = f();
                } catch (...) {
                    std::unique_lock sl(shared->mtx);
                    shared->error = std::current_exception();
                    shared->done = true;
                    auto w = shared->waiter;
                    if (w) {
                        if (post_cb)
                            post_cb(w);
                        else
                            w.resume();
                    }
                    return;
                }
                std::unique_lock sl(shared->mtx);
                shared->result.emplace(std::move(value));
                shared->done = true;
                auto w = shared->waiter;
                if (w) {
                    if (post_cb)
                        post_cb(w);
                    else
                        w.resume();
                }
            });
        }
        cv_.notify_one();

        struct Awaiter {
            std::shared_ptr<Shared> shared;

            bool await_ready() {
                std::unique_lock lock(shared->mtx);
                return shared->done;
            }

            bool await_suspend(std::coroutine_handle<> h) {
                std::unique_lock lock(shared->mtx);
                shared->waiter = h;
                return !shared->done;
            }

            R await_resume() {
                if (shared->error)
                    std::rethrow_exception(shared->error);
                return std::move(*shared->result);
            }
        };

        co_return co_await Awaiter{std::move(shared)};
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });
                if (stop_ && tasks_.empty())
                    return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    PostCallback post_cb_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;
};

} // namespace servd
