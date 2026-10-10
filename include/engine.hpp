#pragma once

#include "order.hpp"
#include "order_book.hpp"
#include "types.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace exchange {

// Pre-allocated pool of orders. Storage grows monotonically (deque push_back
// never invalidates existing pointers) and freed orders are recycled through an
// intrusive free-list, so the matching path performs no heap allocation.
class OrderPool {
public:
    Order* acquire() {
        if (free_list_) {
            Order* o = free_list_;
            free_list_ = o->next;
            o->reset();
            return o;
        }
        storage_.emplace_back();
        return &storage_.back();
    }

    void release(Order* o) {
        o->reset();
        o->next = free_list_;
        free_list_ = o;
    }

    size_t capacity() const { return storage_.size(); }

private:
    std::deque<Order> storage_;
    Order* free_list_ = nullptr;
};

// Request used to submit an order into the engine.
struct OrderRequest {
    OrderID order_id = 0;
    AccountID account_id = 0;
    std::string client_order_id;
    std::string symbol;
    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    Price price = 0;
    Quantity quantity = 0;
    TimeInForce tif = TimeInForce::GTC;
};

// Result of a place-order call.
struct PlaceResult {
    bool accepted = false;
    std::string reject_reason;
    OrderID order_id = 0;
    OrderStatus status = OrderStatus::New;
    Price average_price = 0;
    std::vector<Trade> trades;
};

// Minimal ticker view for market data.
struct Ticker {
    std::string symbol;
    Price last_price = 0;
    Price best_bid = 0;
    Price best_ask = 0;
};

// The matching engine: owns one OrderBook per symbol plus a global order index
// and id generation.
class MatchingEngine {
public:
    MatchingEngine();

    PlaceResult place_order(const OrderRequest& req);

    // Cancel a resting order. Returns false if the order is unknown or already
    // terminal.
    bool cancel_order(OrderID order_id, const std::string& symbol);

    // L2 depth snapshot.
    OrderBookSnapshot snapshot(const std::string& symbol, uint32_t depth) const;

    // Ticker (last price + best bid/ask) for a symbol.
    Ticker ticker(const std::string& symbol) const;

    // Open (NEW / PARTIALLY_FILLED) orders, optionally filtered by symbol.
    std::vector<OrderSnapshot> open_orders(const std::string& symbol) const;

    OrderID next_order_id() { return ++next_order_id_; }
    TradeID next_trade_id() { return ++next_trade_id_; }

private:
    OrderBook& book_for(const std::string& symbol);

    std::unordered_map<std::string, OrderBook> books_;
    std::unordered_map<OrderID, Order*> order_index_;
    std::unordered_map<std::string, Price> last_prices_;
    OrderPool pool_;
    OrderID next_order_id_ = 0;
    TradeID next_trade_id_ = 0;
    uint64_t sequence_ = 0;
};

} // namespace exchange
