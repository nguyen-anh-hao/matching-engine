#pragma once

#include "engine.hpp"

#include <cstdint>
#include <mutex>

namespace exchange {

// Minimal HTTP/JSON server that exposes the matching engine over REST:
//   POST   /orders             -> place order
//   DELETE /orders             -> cancel order
//   GET    /book?symbol&depth  -> L2 snapshot
//   GET    /ticker?symbol      -> last price + best bid/ask
//   GET    /health             -> liveness
// The transport is intentionally small and dependency-free (raw sockets) so the
// engine can be dropped into a production C++ stack with cpp-httplib/Crow later.
class HttpServer {
public:
    HttpServer(uint16_t port, MatchingEngine& engine)
        : port_(port), engine_(engine) {}

    // Blocking accept loop. Returns when stop() is called from another thread.
    void run();

    void stop() { running_ = false; }

private:
    uint16_t port_;
    MatchingEngine& engine_;
    volatile bool running_ = true;
    intptr_t listen_fd_ = -1;

    // The matching core is single-threaded by design; this mutex serializes
    // access to it while HTTP I/O runs concurrently.
    std::mutex engine_mutex_;
};

} // namespace exchange
