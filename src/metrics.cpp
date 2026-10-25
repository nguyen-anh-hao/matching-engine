#include "metrics.hpp"

#include <sstream>

namespace exchange {

Metrics::Metrics() {
    for (auto& bucket : latency_buckets_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

void Metrics::begin_connection() {
    active_connections_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::end_connection() {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
}

void Metrics::record_order() {
    orders_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::record_trades(size_t count) {
    trades_.fetch_add(count, std::memory_order_relaxed);
}

void Metrics::record_cancel() {
    cancels_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::record_rejection() {
    errors_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::record_throttled() {
    errors_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::record_latency(uint64_t elapsed_ns) {
    latency_sum_ns_.fetch_add(elapsed_ns, std::memory_order_relaxed);
    latency_count_.fetch_add(1, std::memory_order_relaxed);
    for (size_t i = 0; i < kLatencyBuckets; ++i) {
        if (elapsed_ns <= kLatencyBoundsNs[i]) {
            latency_buckets_[i].fetch_add(1, std::memory_order_relaxed);
        }
    }
}

std::string Metrics::render(size_t pool_capacity) const {
    std::ostringstream out;
    out << "# HELP trading_matching_orders_total Orders submitted.\n"
        << "# TYPE trading_matching_orders_total counter\n"
        << "trading_matching_orders_total " << orders_.load(std::memory_order_relaxed) << "\n"
        << "# HELP trading_matching_active_connections Active HTTP connections.\n"
        << "# TYPE trading_matching_active_connections gauge\n"
        << "trading_matching_active_connections " << active_connections_.load(std::memory_order_relaxed)
        << "\n"
        << "# HELP trading_matching_trades_total Trades executed.\n"
        << "# TYPE trading_matching_trades_total counter\n"
        << "trading_matching_trades_total " << trades_.load(std::memory_order_relaxed) << "\n"
        << "# HELP trading_matching_cancels_total Cancels accepted.\n"
        << "# TYPE trading_matching_cancels_total counter\n"
        << "trading_matching_cancels_total " << cancels_.load(std::memory_order_relaxed) << "\n"
        << "# HELP trading_matching_errors_total Rejected orders, failed cancels and throttled "
           "connections.\n"
        << "# TYPE trading_matching_errors_total counter\n"
        << "trading_matching_errors_total " << errors_.load(std::memory_order_relaxed) << "\n"
        << "# HELP trading_matching_order_pool_capacity Order slots allocated by the pool.\n"
        << "# TYPE trading_matching_order_pool_capacity gauge\n"
        << "trading_matching_order_pool_capacity " << pool_capacity << "\n"
        << "# HELP trading_matching_latency_seconds Matching latency.\n"
        << "# TYPE trading_matching_latency_seconds histogram\n";

    constexpr double kNanosecondsPerSecond = 1'000'000'000.0;
    for (size_t i = 0; i < kLatencyBuckets; ++i) {
        out << "trading_matching_latency_seconds_bucket{le=\""
            << static_cast<double>(kLatencyBoundsNs[i]) / kNanosecondsPerSecond << "\"} "
            << latency_buckets_[i].load(std::memory_order_relaxed) << "\n";
    }
    out << "trading_matching_latency_seconds_bucket{le=\"+Inf\"} "
        << latency_count_.load(std::memory_order_relaxed) << "\n"
        << "trading_matching_latency_seconds_sum "
        << static_cast<double>(latency_sum_ns_.load(std::memory_order_relaxed)) / kNanosecondsPerSecond
        << "\n"
        << "trading_matching_latency_seconds_count " << latency_count_.load(std::memory_order_relaxed)
        << "\n";
    return out.str();
}

} // namespace exchange
