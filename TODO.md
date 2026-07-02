# servd — Roadmap

This document lists known limitations, planned features, and ideas for future development. **Pull requests welcome.**

---

## Short Term (v0.2)

| Task | Priority | Status |
|------|----------|--------|
| **Unit tests** — basic framework for testing routes, auth, sessions | High | ⬜ |
| **Integration tests** — automated client/server test harness (Python or C++) | High | ⬜ |
| **Error handling** — standardized error codes in `ResponseFrame.flags` | Medium | ⬜ |
| **Graceful shutdown** — drain active connections before `stop()` completes | Medium | ⬜ |
| **Connection pool** — track `UringTcpConnection` objects by fd, not just session_id → fd map | Medium | ⬜ |
| **CMake install rules** — `make install` for library + headers | Medium | ⬜ |
| **Doxygen-compatible comments** — public API self-documentation | Low | ⬜ |

---

## Medium Term (v0.3–v0.4)

| Task | Priority | Status |
|------|----------|--------|
| **TLS/SSL** — wrap TCP connections with OpenSSL/BoringSSL transport | High | ⬜ |
| **Rate limiting** — per-session or per-IP command throttling | Medium | ⬜ |
| **Connection metadata** — expose remote address, port, and TLS cipher in `Context` | Medium | ⬜ |
| **Middleware pipeline** — chain of `IAuthenticator`-like filters before the handler | Medium | ⬜ |
| **Client disconnect callback** — hook when a session disconnects | Medium | ⬜ |
| **Request cancellation** — allow a handler to cancel an in-flight request | Medium | ⬜ |
| **Payload compression** — optional zstd/lz4 in `flags` | Low | ⬜ |
| **Config hot-reload** — re-read `servd.conf` on `SIGHUP` | Low | ⬜ |

---

## Long Term (v0.5+)

| Task | Priority | Status |
|------|----------|--------|
| **Multithreaded io_uring** — multiple SQ/CQ rings with work stealing | Low | ⬜ |
| **WebSocket compatibility** — frame wrapping for browser clients | Low | ⬜ |
| **Unix socket peer credentials** — `SO_PEERCRED` for local auth | Low | ⬜ |
| **Windows / macOS support** — abstract I/O behind a platform layer (IOCP, kqueue) | Low | ⬜ |
| **Service mesh integration** — mTLS, SPIFFE, sidecar pattern | Low | ⬜ |
| **Metrics / observability** — Prometheus endpoints, OpenTelemetry traces | Low | ⬜ |

---

## Known Limitations (v0.1)

1. **No TLS** — all traffic is plaintext. Do not use over untrusted networks.
2. **No tests** — the project currently has zero automated tests.
3. **Single-threaded** — one thread handles all I/O. CPU-bound handlers will block the entire server.
4. **All handlers must be coroutines** — `Handler` returns `Task<ResponseFrame>`. Blocking inside a handler blocks the event loop.
5. **No request timeout** — a handler that never `co_return`s hangs the client forever.
6. **Router is brute-force** — `std::array<Endpoint, 65536>` is simple but wastes ~4 MB for unused slots.
7. **`InMemorySessionStore` is not persisted** — all sessions are lost on restart. Bring your own `ISessionStore` for persistence.
8. **Discovery UDP broadcast only** — limited to a single LAN segment (no multicast routing).
9. **Linux-only** — depends on `io_uring` (Linux ≥ 5.1).
10. **`CAP_NET_ADMIN` required** — for discovery broadcast socket options.

---

## How to Contribute

1. Fork the repository
2. Pick a task from this list (or suggest a new one)
3. Implement it, keeping the existing code style and performance constraints
4. Submit a pull request

**Code style rules:**
- C++20, no exceptions in the hot path
- No heap allocation in the request-processing pipeline (pre-allocate where needed)
- No virtual dispatch in the hot path (except via injected interfaces, which are called once per request)
- Comments in English, code in English, variable names in English
- Follow the existing header comment format
