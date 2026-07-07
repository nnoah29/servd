# servd Architecture

This document describes the internal architecture of servd in detail. It is intended for developers who want to understand, extend, or debug the framework.

---

## 1. Design Philosophy

servd follows three core principles:

1. **Separation of Concerns** — The framework handles all I/O, sessions, authentication dispatching, and transport. The developer provides only business logic (handlers, authenticators, session stores).

2. **Inversion of Control (IoC)** — The framework owns the event loop and calls user code via injected interfaces (`IAuthenticator`, `ISessionStore`) and registered handlers. The developer never touches a file descriptor.

3. **Zero-Copy, Zero-Allocation Hot Path** — The frame payload is delivered as a `std::span<const std::byte>` pointing directly into the receive buffer. No allocation, no parsing. The `Context` object lives on the coroutine stack frame.

---

## 2. High-Level Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     Developer's Application                   │
│  (app/main.cpp)                                              │
│  add_command(handler) .require_auth() .tcp_only()            │
│  set_authenticator(MyAuth)                                   │
│  set_session_store(MyStore)                                  │
│  add_periodic_task(...)                                      │
│  init() / run() / stop()                                     │
└────────────────────────┬─────────────────────────────────────┘
                         │ calls
┌────────────────────────▼─────────────────────────────────────┐
│                     servd::Server                              │
│  ┌──────────┐  ┌──────────────┐  ┌──────────────────────┐    │
│  │  Router   │  │ SessionStore │  │   Authenticator      │    │
│  │ (O(1)     │  │ (injectable) │  │   (injectable)       │    │
│  │  array)   │  └──────────────┘  └──────────────────────┘    │
│  └──────────┘                                                 │
│                         │ owns                                │
└─────────────────────────┼─────────────────────────────────────┘
                          │
┌─────────────────────────▼─────────────────────────────────────┐
│                  UringEngine (detail/Engine.hpp)               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  io_uring Event Loop (single thread, no mutex)          │  │
│  │                                                         │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │  │
│  │  │ accept loop  │  │  TCP client  │  │  UDP loop     │  │  │
│  │  │ (per fd)     │  │  handlers    │  │  (per fd)     │  │  │
│  │  └─────────────┘  └──────────────┘  └───────────────┘  │  │
│  │                                                         │  │
│  │  ┌──────────────────────────────────────────────────┐   │  │
│  │  │  Coroutine Lifecycle                             │   │  │
│  │  │  async_accept → handle_client → read_frame       │   │  │
│  │  │  → process_command → session_store.save          │   │  │
│  │  └──────────────────────────────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────┘
```

---

## 3. The io_uring Event Loop

### 3.1 Why io_uring?

Traditional Linux I/O (epoll, select, poll) requires **two syscalls per I/O operation**: one to submit, one to reap. `io_uring` reduces this to **one** by using two shared ring buffers between kernel and userspace:

- **Submission Queue (SQ)** — the application posts I/O requests (read, write, accept, timeout)
- **Completion Queue (CQ)** — the kernel posts completed results

This eliminates syscall overhead entirely for high-throughput workloads.

### 3.2 How Coroutines Wire Into io_uring

Every I/O operation follows this pattern:

```
1. Application calls co_await async_read(fd, buffer)
   ↓
2. async_read allocates an UringOperation on the stack
   ↓
3. A SQE is filled: io_uring_prep_recv(sqe, fd, buffer, size)
         io_uring_sqe_set_data(sqe, &op)
   ↓
4. The coroutine suspends (co_await op)
         → The coroutine handle is stored in op.coroutine
   ↓
5. io_uring_submit(&ring) posts the SQE to the kernel
   ↓
6. Kernel completes the I/O → writes CQE with op as user_data
   ↓
7. run() loop: io_uring_wait_cqe(&ring, &cqe)
         → cqe->user_data == &op
         → op.cqe_res = cqe->res
         → op.coroutine.resume()  // resumes the coroutine
   ↓
8. The coroutine resumes at co_await op, reads op.cqe_res
```

There is exactly **no callback, no lambda, no heap allocation** in this path.

### 3.3 Single-Threaded Design

The entire server runs on **one thread**. There are zero mutexes in the I/O path:
- Each client has its own coroutine stack frame
- Each coroutine suspends at `co_await` points and resumes when its I/O completes
- The `run()` loop is the single scheduler — it processes CQEs and resumes the corresponding coroutines

This avoids all lock contention, false sharing, and context-switching overhead.

---

## 4. The Connection Lifecycle

### 4.1 TCP / Unix Stream

```
start_accept_loop(server_fd)
  │
  ├─ co_await async_accept(server_fd)   ← suspends until client connects
  │
  └─ handle_client(client_fd)           ← new coroutine, detached
       │
       ├─ loop:
       │   ├─ co_await read_frame()     ← read 16-byte header + payload
       │   ├─ session_store_.get_or_create(session_id)
       │   ├─ register_session(session_id, fd)   ← for broadcast
       │   ├─ co_await process_command()         ← routing + auth + handler
       │   └─ session_store_.save(session)
       │
       └─ on disconnect / error:
           unregister_session(session_id)
           close(fd)
```

### 4.2 UDP

```
start_udp_loop(udp_fd)
  │
  ├─ loop:
  │   ├─ co_await async_recvmsg()        ← receive datagram
  │   ├─ parse FrameHeader from buffer
  │   ├─ session_store_.get_or_create(session_id)
  │   ├─ co_await process_command()      ← routing + auth + handler
  │   └─ session_store_.save(session)
```

UDP is connectionless — every datagram creates a temporary `UringUdpConnection` object for the duration of `process_command`.

---

## 5. The Command Processing Pipeline

Inside `process_command()`, the flow is:

```
1. router_.get(command_id)             → O(1) array lookup
   ├─ if nullptr → LOG + co_return (unknown command)
   │
2. Context ctx(header, payload, session, connection)   ← stack allocation
   │
3. if endpoint->requires_auth:
       co_await authenticator_->authenticate(ctx)
   │
4. Transport check:
       endpoint->allowed_transport == ANY || match
   │
5. if !auth_ok || !transport_ok:
       LOG + co_return (rejected)
   │
6. auto [flags, payload] = co_await endpoint->handler(ctx)
   │
7. send response frame back to client
```

---

## 6. Session Persistence

`Session` objects are stored by an injected `ISessionStore`. The framework provides `InMemorySessionStore` as default.

The lifecycle:
1. Every incoming frame carries a `session_id` (set by the client)
2. The engine calls `store->get_or_create(session_id)` — either returns existing or creates a new one
3. The developer can modify the session inside the handler:
   ```cpp
   ctx.session().set_authenticated(true, "alice");
   ```
4. After the handler completes, the engine calls `store->save(session)`

If the client reconnects with the same `session_id` (over UDP or a new TCP connection), their session state is preserved.

---

## 7. Authentication Strategy Pattern

servd uses the **Strategy pattern** for authentication, exactly like `ISessionStore`:

```
IAuthenticator (interface)
  │
  ├── DefaultAuthenticator     ← checks session.is_authenticated()
  │
  └── YourCustomAuth           ← database, JWT, HMAC, etc.
```

The `authenticate()` method receives the full `Context`, giving it access to:
- The session (`ctx.session()`)
- The raw payload (`ctx.payload()`)
- The transport type (`ctx.current_transport()`)
- The property bag (`ctx.set<T>()` / `ctx.get<T>()`)

This allows the authenticator to pass computed data to the handler without re-querying:

```cpp
// In authenticator:
ctx.set("user", database_result);

// In handler:
auto user = ctx.get<User>("user");
```

---

## 8. The Property Bag (std::any)

The `Context` class contains a `std::unordered_map<std::string, std::any>` called `locals_`.

- **Why not inheritance?** In C++, inheritance would require heap allocation and virtual dispatch. The `Context` is stack-allocated in `process_command()` for maximum performance.

- **Why `std::any`?** It can hold any type safely. The `get<T>()` method performs runtime type checking and throws `std::bad_any_cast` on mismatch.

- **Lifetime:** The property bag lives exactly as long as the `Context` object — from after `process_command()` creates it until the handler returns. It is destroyed when the coroutine frame is destroyed, so no manual cleanup is needed.

---

## 9. The Binary Router

The `Router` class uses a **fixed-size `std::array<Endpoint, 65536>`**, indexed directly by `command_id`.

```cpp
class Router {
    std::array<Endpoint, 65536> routes_;

    Endpoint& add(uint16_t command_id, const Handler& handler) {
        routes_[command_id].handler = handler;
        return routes_[command_id];
    }

    const Endpoint* get(uint16_t command_id) const {
        if (!routes_[command_id].is_valid()) return nullptr;
        return &routes_[command_id];
    }
};
```

**Performance:** O(1), single array access, no hash computation, no branching (the `is_valid()` check compiles to a null check on the `std::function`).

**Trade-off:** The array is 65536 × sizeof(Endpoint) ≈ 4 MB (with `std::function`). This is acceptable for a server framework but worth noting for memory-constrained environments.

---

## 10. Transport Abstraction

The `IConnection` interface abstracts the transport layer:

```cpp
class IConnection {
    virtual TransportType transport_type() const = 0;
    virtual Task<void> send_frame(const FrameHeader&, bytes payload) = 0;
    virtual std::string get_remote_address() const = 0;
};
```

Two implementations live inside `UringEngine`:

| Class | Transport | send_frame impl |
|-------|-----------|----------------|
| `UringTcpConnection` | TCP / Unix | `async_write(fd, buffer)` |
| `UringUdpConnection` | UDP | `async_sendto(fd, buffer, addr)` |

Adding a new transport (e.g., TLS) means implementing `IConnection` and plugging it into the appropriate read loop.

---

## 11. Discovery Protocol

The discovery system uses a **9-byte UDP broadcast packet**:

```
┌──────────────────────────────────────────────────────┐
│  uint32_t magic_number  │ uint8_t action │ int32_t:p │
│  (4 bytes)              │  (1 byte)      │ (4 bytes) │
└──────────────────────────────────────────────────────┘
```

- **CLIENT_LOOKING_FOR_SERVER (0x01):** A client broadcasts this. The server responds with its TCP/UDP ports.
- **SERVER_ANNOUNCING (0x02):** The server broadcasts its presence periodically (if configured with `active_announce_if_idle`).

---

## 12. File Layout and Dependencies

```
include/servd/interfaces/    ← Pure abstract classes (IAuthenticator, IConnection, ISessionStore, Session)
include/servd/router/        ← Router + Endpoint
include/servd/store/         ← InMemorySessionStore
include/servd/auth/          ← DefaultAuthenticator
include/servd/               ← Server, Context, Protocol, Task
src/detail/Engine.hpp        ← UringEngine definition (private to src/)
src/UringEngine.cpp          ← io_uring implementation, IConnection implementations
src/Server.cpp               ← Server public API implementation
```

Dependencies:
- **`liburing`** — io_uring syscall wrappers (only dependency)
- **C++20 Standard Library** — coroutines, any, span, format

No Boost, no OpenSSL, no protobuf.

### Concurrency — `when_all` and `co_start`

Two higher-level concurrency helpers are built on top of `Task<T>`:

- **`when_all(tasks...)`** — launches N coroutines concurrently (bypassing `Task`'s lazy `initial_suspend`). Each task is wrapped in a small internal `WhenAllTask<T>` coroutine that `co_await`s the original task, stores the result in a shared state, and atomically decrements a counter in its `final_suspend`. When the counter reaches zero, the last wrapper resumes the `when_all` caller. Exceptions from any task are captured and rethrown.

- **`co_start(task)`** — eagerly resumes a `Task<T>` without setting a continuation, useful for fire-and-forget scenarios. The `Task` object must outlive the started coroutine.

---

## 13. Threading Model

### 13.1 Event Loop Thread

```
Single thread runs:
├─ run()                   ← waits for CQEs, resumes coroutines
├─ Coroutine A (TCP client)
├─ Coroutine B (TCP client)
├─ Coroutine C (UDP loop)
├─ Coroutine D (accept loop)
└─ Coroutine E (periodic timer)
```

All coroutines are interleaved on the same thread. The `run()` loop acts as a cooperative scheduler — each coroutine yields at `co_await` points and resumes when its I/O completes.

**There is no preemption and no mutex in the hot path.** This keeps the design simple and eliminates concurrency bugs.

### 13.2 Optional Thread Pool for Blocking Operations

When `Server::enable_thread_pool(N)` is called, servd creates a `ThreadPool` with N dedicated worker threads. This pool exists solely to offload **blocking system calls** (`popen`, `opendir`, `getpwuid`, ALSA `snd_mixer_*`, etc.) that would otherwise stall the entire event loop.

```
Worker thread 1 ──┐
Worker thread 2 ──┤  execute f(), store result,
Worker thread 3 ──┤  submit io_uring NOP to wake
Worker thread 4 ──┘  the waiting coroutine
```

**Thread handoff protocol:**

```
Event loop thread                    Worker thread
        │                                 │
        ├─ co_await pool.enqueue(f)        │
        │  ├─ submit f() to queue          │
        │  └─ suspend (waits)              │
        │                                 ├─ pick up f()
        │                                 ├─ execute f()
        │                                 ├─ store result in shared state
        │                                 └─ post io_uring NOP
        │                                      │
        ├─ CQE: NOP completes                  │
        ├─ resume waiting coroutine            │
        └─ continue processing result          │
```

The worker thread does **not** directly call `coroutine_handle::resume()` — it submits an `io_uring NOP` operation to the kernel ring. The event loop picks up the resulting CQE on its own thread and resumes the coroutine. This guarantees that **all coroutine execution stays on the event-loop thread**, preserving the single-threaded programming model.

### 13.3 Thread Safety Summary

| Resource | Shared between | Protection |
|----------|---------------|------------|
| `io_uring` SQ (from event loop) | Event loop only | None needed |
| `io_uring` SQ (from workers) | Workers + event loop | `sqe_mutex_` in `UringEngine` |
| Worker task queue | All workers | `std::mutex` + `std::condition_variable` |
| Coroutine shared state (per `enqueue`) | 1 worker + event loop | Per-operation `std::mutex` |
| Session store / authenticator | Event loop only | None needed |
