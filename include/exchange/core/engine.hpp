#pragma once

#include "exchange/core/engine_types.hpp"
#include "exchange/core/market_data.hpp"
#include "exchange/core/order.hpp"
#include "exchange/core/order_book.hpp"
#include "exchange/core/order_pool.hpp"
#include "exchange/core/trade.hpp"
#include "exchange/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace exchange {

// The matching engine: owns one OrderBook per symbol plus a global order index
// and id generation. It is intentionally single-writer; callers serialize
// access (the HTTP server does so with a mutex).
class MatchingEngine {
public:
    MatchingEngine() = default;

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;

    PlaceResult place_order(const OrderRequest& request);

    // Cancel a resting order. Returns false if the order is unknown, not open,
    // or belongs to a different symbol.
    bool cancel_order(OrderID order_id, const std::string& symbol);

    // L2 depth snapshot. depth == 0 returns the full book.
    OrderBookSnapshot snapshot(const std::string& symbol, uint32_t depth) const;

    // Ticker (last price + best bid/ask) for a symbol.
    Ticker ticker(const std::string& symbol) const;

    // Open (NEW / PARTIALLY_FILLED) orders, optionally filtered by symbol,
    // ordered by match sequence.
    std::vector<OrderSnapshot> open_orders(const std::string& symbol) const;

    // Number of order slots ever allocated by the pool. Constant in steady
    // state when orders are cancelled/filled; exposed as a metrics gauge.
    size_t order_pool_capacity() const { return pool_.capacity(); }

private:
    OrderID next_order_id() { return ++next_order_id_; }
    TradeID next_trade_id() { return ++next_trade_id_; }

    void release_filled_makers(const std::vector<Trade>& trades);
    void finalize_order(Order* incoming, OrderBook& book, PlaceResult& result);

    static constexpr size_t kMaxBookSymbols = 4096;

    std::unordered_map<std::string, OrderBook> books_;
    std::unordered_map<OrderID, Order*> order_index_;
    std::unordered_map<std::string, Price> last_prices_;
    OrderPool pool_;
    OrderID next_order_id_ = 0;
    TradeID next_trade_id_ = 0;
    uint64_t sequence_ = 0;
};

} // namespace exchange
