#pragma once

#include "engine.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace exchange {

// Minimal HTTP/JSON server that exposes the matching engine over REST:
//   POST   /orders             -> place order
//   DELETE /orders             -> cancel order
//   GET    /book?symbol&depth  -> L2 snapshot
//   GET    /ticker?symbol      -> last price + best bid/ask
//   GET    /health             -> liveness
//   GET    /metrics            -> Prometheus metrics
// The transport is intentionally small and dependency-free (raw sockets) so the
// engine can be dropped into a production C++ stack with cpp-httplib/Crow later.
class HttpServer {
public:
    HttpServer(uint16_t port, MatchingEngine& engine)
        : port_(port), engine_(engine) {
        for (auto& b : latency_buckets_) b.store(0, std::memory_order_relaxed);
    }

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

    // Prometheus metrics (atomic; no locks on the matching path).
    std::atomic<uint64_t> orders_placed_{0};
    std::atomic<uint64_t> active_connections_{0};
    std::atomic<uint64_t> trades_executed_{0};
    std::atomic<uint64_t> cancels_{0};
    std::atomic<uint64_t> errors_{0};
    std::atomic<uint64_t> latency_sum_ns_{0};
    std::atomic<uint64_t> latency_count_{0};
    static constexpr size_t kLatencyBuckets = 13;
    std::array<std::atomic<uint64_t>, kLatencyBuckets> latency_buckets_{};
};

} // namespace exchange
