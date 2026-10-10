#pragma once

#include "limit_level.hpp"
#include "order.hpp"
#include "types.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace exchange {

// A single executed trade between a maker (resting order) and a taker (incoming).
struct Trade {
    TradeID id = 0;
    std::string symbol;
    Price price = 0;
    Quantity quantity = 0;
    OrderID maker_order_id = 0;
    OrderID taker_order_id = 0;
    AccountID maker_account_id = 0;
    AccountID taker_account_id = 0;
    int64_t timestamp_ns = 0;
};

// Read-only projection of an order (used by open-orders / history queries).
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

using BidMap = std::map<Price, LimitLevel, std::greater<Price>>;
using AskMap = std::map<Price, LimitLevel, std::less<Price>>;

// Price-time priority order book for a single symbol.
class OrderBook {
public:
    explicit OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

    // Match an incoming order against the resting book, filling FIFO within
    // price levels. Appends any resulting trades. After this returns, `incoming`
    // may still have remaining `leaves` (for GTC/IOC) or be fully filled.
    void match(Order* incoming, std::vector<Trade>& trades);

    // Insert a remaining (resting) order into the book.
    void add(Order* o);

    // Remove an order from the book. Returns true if it was present.
    bool remove(Order* o);

    Order* find(OrderID id) const;

    // Whether there is enough opposite liquidity to fully fill the order at
    // once (used by FOK pre-check).
    bool can_fill_completely(const Order* o) const;

    const BidMap& bids() const { return bids_; }
    const AskMap& asks() const { return asks_; }
    const std::string& symbol() const { return symbol_; }

private:
    std::string symbol_;
    BidMap bids_;
    AskMap asks_;
    std::unordered_map<OrderID, Order*> index_;
};

} // namespace exchange
