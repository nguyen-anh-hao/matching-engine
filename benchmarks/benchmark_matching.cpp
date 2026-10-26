// Throughput/latency benchmark for the matching engine. Measures nanoseconds per
// order when continuously crossing a single symbol's book. A warm-up engine runs
// first so the measurement does not include cold caches or allocator growth.
#include "exchange/core/engine.hpp"

#include <chrono>
#include <cstdio>

using namespace exchange;

namespace {

OrderRequest make_request(Side side, Price price, Quantity quantity, AccountID account) {
    OrderRequest request;
    request.symbol = "BENCH";
    request.side = side;
    request.type = OrderType::Limit;
    request.price = price;
    request.quantity = quantity;
    request.account_id = account;
    return request;
}

// Returns the number of orders that were unexpectedly rejected.
int run_batch(MatchingEngine& engine, const OrderRequest& buy, int iterations) {
    int rejected = 0;
    for (int i = 0; i < iterations; ++i) {
        if (!engine.place_order(buy).accepted)
            ++rejected;
    }
    return rejected;
}

} // namespace

int main() {
    constexpr int kIterations = 200'000;
    constexpr int kWarmupIterations = 1'000;

    const OrderRequest seed =
        make_request(Side::Sell, 100 * kScale, static_cast<Quantity>(kIterations) * kScale, 1);
    const OrderRequest buy = make_request(Side::Buy, 100 * kScale, kScale, 2);

    // Warm-up run (result ignored).
    {
        MatchingEngine warmup;
        warmup.place_order(seed);
        run_batch(warmup, buy, kWarmupIterations);
    }

    MatchingEngine engine;
    const auto seeded = engine.place_order(seed);
    if (!seeded.accepted) {
        std::printf("seed order rejected: %s\n", seeded.reason.c_str());
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    const int rejected = run_batch(engine, buy, kIterations);
    const auto end = std::chrono::steady_clock::now();

    const double total_ns =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    const double avg_ns = total_ns / static_cast<double>(kIterations);
    const double orders_per_second = total_ns > 0.0 ? static_cast<double>(kIterations) * 1e9 / total_ns : 0.0;

    std::printf("orders=%d total=%.0fns avg=%.2fns/order throughput=%.0forders/s rejected=%d\n", kIterations,
                total_ns, avg_ns, orders_per_second, rejected);
    return rejected == 0 ? 0 : 1;
}
