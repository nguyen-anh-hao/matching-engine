#include <chrono>
#include <iostream>
#include <vector>

#include "async_wal.hpp"
#include "fast_order_book.hpp"
#include "lockfree_spsc_queue.hpp"
#include "network_receiver.hpp"
#include "types.hpp"

#if defined(__x86_64__)
#include <immintrin.h>
#endif

constexpr size_t EXPECTED_ORDERS = 1'000'000;

int main() {
    std::cout << "Starting Phase 3 Full Pipeline Server (UDP + Lock-free Book + io_uring WAL)...\n";

    SpscQueue<IngestOrderCommand, 131072> ingress_queue;
    FastOrderBook order_book;
    AsyncWalLogger wal("engine_trades.wal");
    std::vector<TradeRecord> trade_sink;
    trade_sink.reserve(1024);

    NetworkReceiver receiver(UDP_PORT, ingress_queue);
    receiver.start();

    std::cout << "Server ready on UDP port " << UDP_PORT << ". Waiting for orders...\n";

    size_t processed_orders = 0;
    size_t total_trades = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    bool benchmark_started = false;

    IngestOrderCommand cmd;
    while (processed_orders < EXPECTED_ORDERS) {
        if (ingress_queue.pop(cmd)) {
            if (!benchmark_started) {
                start_time = std::chrono::high_resolution_clock::now();
                benchmark_started = true;
            }

            trade_sink.clear();
            order_book.process_order(cmd, trade_sink);

            for (const auto& trade : trade_sink) {
                wal.append_trade(trade);
                total_trades++;
            }
            processed_orders++;
        } else {
#if defined(__x86_64__)
            _mm_pause();
#endif
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    wal.flush_pending();
    receiver.stop();

    double throughput = static_cast<double>(processed_orders) / elapsed.count();
    double avg_latency = (elapsed.count() * 1e6) / processed_orders;

    std::cout << "\n--- PHASE 3 E2E BENCHMARK RESULTS ---\n";
    std::cout << "Orders Processed   : " << processed_orders << "\n";
    std::cout << "Trades Generated   : " << total_trades << "\n";
    std::cout << "Elapsed Time       : " << elapsed.count() << " seconds\n";
    std::cout << "Throughput         : " << throughput / 1e6 << " M orders/sec\n";
    std::cout << "Avg E2E Latency    : " << avg_latency << " µs / order\n";

    unlink("engine_trades.wal");  // Dọn dẹp WAL file
    return 0;
}