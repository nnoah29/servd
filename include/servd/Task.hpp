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
#include <tuple>
#include <vector>
#include <memory>
#include <optional>

namespace servd {

template<typename T = void>
class Task;

template<typename... Ts>
Task<std::tuple<Ts...>> when_all(Task<Ts>... tasks);

template<typename T>
Task<std::vector<T>> when_all(std::vector<Task<T>> tasks);

namespace detail {

struct WhenAllStateBase {
    int remaining;
    std::exception_ptr error;
    std::coroutine_handle<> caller;
    bool started = false;
};

template<typename T>
struct WhenAllSlot {
    std::optional<T> value;
};

template<typename... Ts>
struct WhenAllState : WhenAllStateBase {
    std::tuple<WhenAllSlot<Ts>...> slots;
};

template<typename T>
struct WhenAllTask {
    struct promise_type {
        WhenAllStateBase* state;

        promise_type() : state(nullptr) {}

        WhenAllTask get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            WhenAllStateBase* state;
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type>) noexcept {
                if (--state->remaining == 0) {
                    if (state->started && state->caller)
                        return state->caller;
                }
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {state}; }

        void return_void() {}

        void unhandled_exception() {
            state->error = std::current_exception();
        }
    };

    std::coroutine_handle<promise_type> handle;
};

template<typename T, typename State>
WhenAllTask<T> make_wrapper(
    std::shared_ptr<State> state,
    WhenAllSlot<T>* slot,
    Task<T> task
) {
    T result = co_await std::move(task);
    slot->value.emplace(std::move(result));
    co_return;
}

template<typename... Ts>
struct WhenAllAwaiter {
    std::shared_ptr<WhenAllState<Ts...>> state;
    std::vector<std::coroutine_handle<>> wrapper_handles;

    bool await_ready() noexcept { return false; }

    bool await_suspend(std::coroutine_handle<> caller) noexcept {
        state->caller = caller;
        for (auto& w : wrapper_handles)
            w.resume();
        state->started = true;
        return state->remaining != 0;
    }

    std::tuple<Ts...> await_resume() {
        for (auto& w : wrapper_handles)
            if (w) w.destroy();
        wrapper_handles.clear();
        if (state->error)
            std::rethrow_exception(state->error);
        return extract(std::index_sequence_for<Ts...>{});
    }

private:
    template<size_t... Is>
    std::tuple<Ts...> extract(std::index_sequence<Is...>) {
        return std::make_tuple(
            std::move(*std::get<Is>(state->slots).value)...
        );
    }
};

} // namespace detail

template<typename T>
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
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
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

    void start() {
        handle_.resume();
    }

private:
    std::coroutine_handle<promise_type> handle_;

    template<typename... Us>
    friend Task<std::tuple<Us...>> when_all(Task<Us>... tasks);

    template<typename U>
    friend Task<std::vector<U>> when_all(std::vector<Task<U>> tasks);
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
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
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

    void start() {
        handle_.resume();
    }

private:
    std::coroutine_handle<promise_type> handle_;

    template<typename... Us>
    friend Task<std::tuple<Us...>> when_all(Task<Us>... tasks);

    template<typename U>
    friend Task<std::vector<U>> when_all(std::vector<Task<U>> tasks);
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

template<typename... Ts>
Task<std::tuple<Ts...>> when_all(Task<Ts>... tasks) {
    using State = detail::WhenAllState<Ts...>;

    auto state = std::make_shared<State>();
    state->remaining = static_cast<int>(sizeof...(Ts));
    state->caller = nullptr;

    std::vector<std::coroutine_handle<>> wrappers;
    wrappers.reserve(sizeof...(Ts));

    [&]<size_t... Is>(std::index_sequence<Is...>) {
        ((void)([&]() {
            auto wrapper = detail::make_wrapper<Ts>(
                state,
                &std::get<Is>(state->slots),
                std::move(tasks)
            );
            wrapper.handle.promise().state = state.get();
            wrappers.push_back(wrapper.handle);
        }()), ...);
    }(std::index_sequence_for<Ts...>{});

    co_return co_await detail::WhenAllAwaiter<Ts...>{
        std::move(state), std::move(wrappers)
    };
}

template<typename T>
Task<std::vector<T>> when_all(std::vector<Task<T>> tasks) {
    struct VectorState : detail::WhenAllStateBase {
        std::vector<std::optional<T>> slots;
    };

    const auto n = static_cast<int>(tasks.size());
    auto state = std::make_shared<VectorState>();
    state->remaining = n;
    state->caller = nullptr;
    state->slots.resize(tasks.size());

    std::vector<std::coroutine_handle<>> wrappers;
    wrappers.reserve(tasks.size());

    for (int i = 0; i < n; ++i) {
        auto wrapper = detail::make_wrapper<T>(
            state, &state->slots[i], std::move(tasks[i])
        );
        wrapper.handle.promise().state = state.get();
        wrappers.push_back(wrapper.handle);
    }

    struct VectorAwaiter {
        std::shared_ptr<VectorState> state;
        std::vector<std::coroutine_handle<>> wrapper_handles;

        bool await_ready() noexcept { return false; }

        bool await_suspend(std::coroutine_handle<> caller) noexcept {
            state->caller = caller;
            for (auto& w : wrapper_handles)
                w.resume();
            state->started = true;
            return state->remaining != 0;
        }

        std::vector<T> await_resume() {
            for (auto& w : wrapper_handles)
                if (w) w.destroy();
            wrapper_handles.clear();
            if (state->error)
                std::rethrow_exception(state->error);
            std::vector<T> results;
            results.reserve(state->slots.size());
            for (auto& slot : state->slots)
                results.push_back(std::move(*slot));
            return results;
        }
    };

    co_return co_await VectorAwaiter{
        std::move(state), std::move(wrappers)
    };
}

template<typename T>
void co_start(Task<T>& task) {
    task.start();
}

} // namespace servd
