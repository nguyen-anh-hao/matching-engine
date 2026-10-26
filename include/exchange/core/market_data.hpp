#pragma once

#include "exchange/core/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace exchange {

// Read-only projection of an order (used by open-orders queries).
struct OrderSnapshot {
    OrderID id = 0;
    AccountID account_id = 0;
    std::string symbol;
    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    Price price = 0;
    Quantity quantity = 0;
    Quantity executed = 0;
    Quantity leaves = 0;
    OrderStatus status = OrderStatus::New;
    Sequence sequence = 0;
};

// One price level of an L2 snapshot.
struct PriceLevel {
    Price price = 0;
    Quantity quantity = 0;
    uint32_t order_count = 0;
};

// Full L2 snapshot for a symbol.
struct OrderBookSnapshot {
    std::string symbol;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    uint64_t sequence = 0;
    int64_t timestamp_ns = 0;
};

// Minimal ticker view for market data.
struct Ticker {
    std::string symbol;
    Price last_price = 0;
    Price best_bid = 0;
    Price best_ask = 0;
};

} // namespace exchange
