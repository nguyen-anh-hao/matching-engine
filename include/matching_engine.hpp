#pragma once
#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "order_book.hpp"

class MatchingEngine {
public:
    explicit MatchingEngine(const std::string& log_filepath);
    ~MatchingEngine();

    void register_symbol(uint32_t symbol_id);
    std::vector<Trade> process_order(const Order& order);

private:
    std::unordered_map<uint32_t, std::unique_ptr<OrderBook>> books_;
    std::mutex engine_mutex_;
    uint64_t trade_id_counter_{0};
    std::ofstream wal_file_;
};