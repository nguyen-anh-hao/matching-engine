#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace exchange {

// Prometheus metrics for the HTTP adapter. All recording is lock-free so it
// never serializes on the engine mutex.
class Metrics {
public:
    Metrics();

    void begin_connection();
    void end_connection();
    void record_order();
    void record_trades(size_t count);
    void record_cancel();
    void record_rejection();
    void record_throttled();
    void record_latency(uint64_t elapsed_ns);

    std::string render(size_t pool_capacity) const;

private:
    static constexpr std::array<uint64_t, 13> kLatencyBoundsNs{
        100'000ULL,     250'000ULL,     500'000ULL,      1'000'000ULL,  2'500'000ULL,
        5'000'000ULL,   10'000'000ULL,  25'000'000ULL,   50'000'000ULL, 100'000'000ULL,
        250'000'000ULL, 500'000'000ULL, 1'000'000'000ULL};
    static constexpr size_t kLatencyBuckets = kLatencyBoundsNs.size();

    std::atomic<uint64_t> orders_{0};
    std::atomic<uint64_t> active_connections_{0};
    std::atomic<uint64_t> trades_{0};
    std::atomic<uint64_t> cancels_{0};
    std::atomic<uint64_t> errors_{0};
    std::atomic<uint64_t> latency_sum_ns_{0};
    std::atomic<uint64_t> latency_count_{0};
    std::array<std::atomic<uint64_t>, kLatencyBuckets> latency_buckets_{};
};

} // namespace exchange
