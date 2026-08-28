#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "fast_order_book.hpp"
#include "lockfree_spsc_queue.hpp"
#include "types.hpp"

#if defined(__x86_64__)
#include <immintrin.h>
#endif

constexpr size_t TOTAL_TEST_ORDERS = 10'000'000;
constexpr size_t QUEUE_SIZE = 131072;  // 2^17

int main() {
    SpscQueue<IngestOrderCommand, QUEUE_SIZE> queue;
    FastOrderBook order_book;
    std::vector<Trade> trade_sink;
    trade_sink.reserve(1024);

    std::cout << "Starting Phase 2 Optimized Benchmark with " << TOTAL_TEST_ORDERS
              << " orders...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    // 1. Thread Producer (Mô phỏng Ingress nạp lệnh)
    std::thread producer([&]() {
        for (uint64_t i = 1; i <= TOTAL_TEST_ORDERS; ++i) {
            IngestOrderCommand cmd;
            cmd.type = IngestOrderCommand::Type::NEW;
            cmd.order_id = i;
            cmd.account_id = 1000 + (i % 100);
            cmd.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
            cmd.price =
                12400 + static_cast<Price>((i % 20) * 10);  // Dao động quanh $124.00 - $126.00
            cmd.quantity = 10;

            while (!queue.push(cmd)) {
#if defined(__x86_64__)
                _mm_pause();
#endif
            }
        }
    });

    // 2. Thread Consumer (Độc quyền xử lý Khớp lệnh - Single Writer Core)
    size_t processed = 0;
    size_t total_trades = 0;

    std::thread consumer([&]() {
        IngestOrderCommand cmd;
        while (processed < TOTAL_TEST_ORDERS) {
            if (queue.pop(cmd)) {
                trade_sink.clear();
                order_book.process_order(cmd, trade_sink);
                total_trades += trade_sink.size();
                processed++;
            } else {
#if defined(__x86_64__)
                _mm_pause();
#endif
            }
        }
    });

    producer.join();
    consumer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    double throughput = static_cast<double>(TOTAL_TEST_ORDERS) / elapsed.count();
    double avg_latency_ns = (elapsed.count() * 1e9) / static_cast<double>(TOTAL_TEST_ORDERS);

    std::cout << "\n--- PHASE 2 OPTIMIZED RESULTS ---\n";
    std::cout << "Total Orders Processed : " << processed << "\n";
    std::cout << "Total Trades Generated : " << total_trades << "\n";
    std::cout << "Elapsed Time           : " << elapsed.count() << " seconds\n";
    std::cout << "Throughput             : " << throughput / 1e6 << " Million orders/sec\n";
    std::cout << "Avg Latency            : " << avg_latency_ns << " ns / order\n";

    return 0;
}