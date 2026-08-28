#include "matching_engine.hpp"

MatchingEngine::MatchingEngine(const std::string& log_filepath) {
    wal_file_.open(log_filepath, std::ios::out | std::ios::app);
}

MatchingEngine::~MatchingEngine() {
    if (wal_file_.is_open()) {
        wal_file_.close();
    }
}

void MatchingEngine::register_symbol(uint32_t symbol_id) {
    std::lock_guard<std::mutex> lock(engine_mutex_);
    if (books_.find(symbol_id) == books_.end()) {
        books_[symbol_id] = std::make_unique<OrderBook>(symbol_id);
    }
}

std::vector<Trade> MatchingEngine::process_order(const Order& order) {
    std::lock_guard<std::mutex> lock(engine_mutex_);

    auto it = books_.find(order.symbol_id);
    if (it == books_.end()) {
        return {};
    }

    std::vector<Trade> trades = it->second->add_order(order, trade_id_counter_);

    if (wal_file_.is_open()) {
        wal_file_ << "ORDER," << order.order_id << "," << order.symbol_id << ","
                  << static_cast<char>(order.side) << "," << order.price << ","
                  << order.initial_quantity << "\n";

        for (const auto& trade : trades) {
            wal_file_ << "TRADE," << trade.trade_id << "," << trade.symbol_id << ","
                      << trade.maker_order_id << "," << trade.taker_order_id << "," << trade.price
                      << "," << trade.quantity << "\n";
        }
        wal_file_.flush();
    }

    return trades;
}