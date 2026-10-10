// Simple throughput/latency benchmark for the matching engine. Measures the
// nanoseconds per order when continuously crossing a single symbol's book.
#include "engine.hpp"

#include <chrono>
#include <cstdio>

using namespace exchange;

int main() {
    constexpr int N = 200000;

    MatchingEngine e;
    // Seed a resting sell order so buys always match.
    OrderRequest seed;
    seed.symbol = "BENCH"; seed.side = Side::Sell; seed.type = OrderType::Limit;
    seed.price = 100 * kScale; seed.quantity = static_cast<Quantity>(N) * kScale;
    seed.account_id = 1;
    e.place_order(seed);

    OrderRequest buy;
    buy.symbol = "BENCH"; buy.side = Side::Buy; buy.type = OrderType::Limit;
    buy.price = 100 * kScale; buy.quantity = kScale; // 1.0
    buy.account_id = 2;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        e.place_order(buy);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double total_ns = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    std::printf("orders=%d total=%.0fns avg=%.2fns/order\n", N, total_ns, total_ns / N);
    return 0;
}
