#pragma once

#include "exchange/core/limit_level.hpp"
#include "exchange/core/order.hpp"
#include "exchange/core/trade.hpp"
#include "exchange/core/types.hpp"

#include <map>
#include <vector>

namespace exchange {

using BidMap = std::map<Price, LimitLevel, std::greater<Price>>;
using AskMap = std::map<Price, LimitLevel, std::less<Price>>;

// Price-time priority order book for a single symbol. The book owns no memory:
// resting orders live in the engine's pool and are indexed by the engine, so
// there is exactly one source of truth for order lookup.
class OrderBook {
public:
    OrderBook() = default;

    // Levels hold raw Order pointers; copying a book would alias them.
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    // Match an incoming order against the resting book, filling FIFO within
    // price levels and skipping the incoming account's own orders (self-trade
    // prevention). Appends any resulting trades. Timestamps and ids are left at
    // zero; the engine stamps them, which keeps the book free of clock reads.
    void match(Order* incoming, std::vector<Trade>& trades);

    // Insert a remaining (resting) order into the book.
    void add(Order* order);

    // Remove an order from the book. Returns false if it was not resting.
    bool remove(Order* order);

    // Whether there is enough opposite liquidity to fully fill the order at
    // once (used by the FOK pre-check).
    bool can_fill_completely(const Order* order) const;

    // True when resting a limit order at its price would cross the opposite
    // side. After match() this can only be caused by self-trade prevention, so
    // the engine refuses to rest such an order.
    bool would_cross(const Order* order) const;

    // Invariant check: best bid must be strictly below best ask.
    bool crossed() const;

    const BidMap& bids() const { return bids_; }
    const AskMap& asks() const { return asks_; }

private:
    BidMap bids_;
    AskMap asks_;
};

} // namespace exchange
