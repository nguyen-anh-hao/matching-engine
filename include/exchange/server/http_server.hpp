#pragma once

#include "exchange/api/handler.hpp"
#include "exchange/core/engine.hpp"
#include "exchange/net/socket.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace exchange {

// HTTP transport: owns the listener, a bounded connection queue and a bounded
// worker pool. Request interpretation lives in ApiHandler; the engine itself
// stays single-writer and is serialized by engine_mutex_.
class HttpServer {
public:
    HttpServer(std::uint16_t port, MatchingEngine& engine);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    // Blocking accept loop. Returns false when the listener cannot be created.
    // Returns true after stop() has drained the worker pool. run() must be
    // joined before this object is destroyed.
    bool run();

    // Thread-safe; the accept loop observes it within one select timeout.
    void stop();

private:
    void worker_loop();
    void handle_connection(net::socket_t client);

    std::uint16_t port_;
    std::atomic<bool> running_{true};

    // Serializes access to the engine while HTTP I/O runs concurrently.
    std::mutex engine_mutex_;
    Metrics metrics_;
    ApiHandler api_;

    // Bounded connection queue between the accept loop and the worker pool.
    static constexpr size_t kMaxQueuedConnections = 1024;
    static constexpr size_t kMinWorkers = 2;
    static constexpr size_t kMaxWorkers = 8;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<net::socket_t> queue_;
    bool stopping_ = false;
};

} // namespace exchange
