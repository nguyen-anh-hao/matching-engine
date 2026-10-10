#include "engine.hpp"

namespace exchange {

MatchingEngine::MatchingEngine() = default;

OrderBook& MatchingEngine::book_for(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end()) {
        it = books_.emplace(symbol, OrderBook(symbol)).first;
    }
    return it->second;
}

PlaceResult MatchingEngine::place_order(const OrderRequest& req) {
    PlaceResult result;
    result.order_id = next_order_id();

    // Validate.
    if (req.quantity <= 0) {
        result.reject_reason = "quantity must be positive";
        result.status = OrderStatus::Rejected;
        return result;
    }
    if (req.type == OrderType::Limit && req.price <= 0) {
        result.reject_reason = "limit price must be positive";
        result.status = OrderStatus::Rejected;
        return result;
    }

    // Allocate the incoming order from the pool (no malloc on the hot path).
    Order* incoming = pool_.acquire();
    incoming->id = result.order_id;
    incoming->account_id = req.account_id;
    incoming->set_symbol(req.symbol);
    incoming->price = req.price;
    incoming->quantity = req.quantity;
    incoming->leaves = req.quantity;
    incoming->executed = 0;
    incoming->side = req.side;
    incoming->type = req.type;
    incoming->tif = req.tif;
    incoming->status = OrderStatus::New;
    incoming->sequence = ++sequence_;

    OrderBook& book = book_for(req.symbol);

    // FOK pre-check: must be fully fillable immediately or it is killed.
    if (req.tif == TimeInForce::FOK && !book.can_fill_completely(incoming)) {
        result.reject_reason = "FOK: insufficient liquidity for full fill";
        result.status = OrderStatus::Cancelled;
        pool_.release(incoming);
        return result;
    }

    // Perform matching against the resting book.
    book.match(incoming, result.trades);

    // Assign trade ids (timestamps were set during match).
    for (Trade& t : result.trades) {
        t.id = next_trade_id();
    }
    if (!result.trades.empty()) {
        last_prices_[req.symbol] = result.trades.back().price;
    }

    // Compute the average fill price for this order (__int128 avoids overflow).
    if (incoming->executed > 0) {
        __int128 notional = 0;
        for (const Trade& t : result.trades) {
            if (t.taker_order_id == incoming->id) {
                notional += static_cast<__int128>(t.price) * t.quantity;
            }
        }
        result.average_price = static_cast<Price>(notional / incoming->executed);
    }

    // Remove resting orders that became fully filled and recycle their memory.
    for (const Trade& t : result.trades) {
        if (t.maker_order_id == incoming->id) continue;
        auto mit = order_index_.find(t.maker_order_id);
        if (mit != order_index_.end()) {
            Order* mo = mit->second;
            if (mo->status == OrderStatus::Filled) {
                order_index_.erase(mit);
                pool_.release(mo);
            }
        }
    }

    // Determine the incoming order's final state.
    if (incoming->leaves == 0) {
        incoming->status = OrderStatus::Filled;
        result.accepted = true;
        result.status = OrderStatus::Filled;
        pool_.release(incoming);
    } else if (incoming->type == OrderType::Market) {
        // A market order cannot rest; leftover leaves means no liquidity.
        incoming->status = OrderStatus::Rejected;
        result.accepted = (incoming->executed > 0);
        result.reject_reason = "market order: insufficient liquidity";
        result.status = OrderStatus::Rejected;
        pool_.release(incoming);
    } else if (incoming->tif == TimeInForce::IOC || incoming->tif == TimeInForce::FOK) {
        // Cancel the remainder of IOC/FOK orders.
        incoming->status = OrderStatus::Cancelled;
        result.accepted = (incoming->executed > 0);
        result.status = OrderStatus::Cancelled;
        pool_.release(incoming);
    } else {
        // GTC limit order: rest the remaining quantity in the book.
        incoming->status = (incoming->executed > 0) ? OrderStatus::PartiallyFilled : OrderStatus::New;
        result.accepted = true;
        result.status = incoming->status;
        book.add(incoming);
        order_index_[incoming->id] = incoming;
    }

    return result;
}

bool MatchingEngine::cancel_order(OrderID order_id, const std::string& symbol) {
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) return false;
    Order* o = it->second;
    if (o->get_symbol() != symbol) return false;

    OrderBook& book = book_for(symbol);
    book.remove(o);
    order_index_.erase(it);
    o->status = OrderStatus::Cancelled;
    pool_.release(o);
    return true;
}

OrderBookSnapshot MatchingEngine::snapshot(const std::string& symbol, uint32_t depth) const {
    OrderBookSnapshot snap;
    snap.symbol = symbol;
    snap.sequence = sequence_;
    snap.timestamp_ns = now_ns();

    auto it = books_.find(symbol);
    if (it == books_.end()) return snap;

    const OrderBook& book = it->second;
    uint32_t n = 0;
    for (const auto& [price, level] : book.bids()) {
        if (depth != 0 && n >= depth) break;
        snap.bids.push_back(PriceLevel{price, level.total_qty(), level.count()});
        ++n;
    }
    n = 0;
    for (const auto& [price, level] : book.asks()) {
        if (depth != 0 && n >= depth) break;
        snap.asks.push_back(PriceLevel{price, level.total_qty(), level.count()});
        ++n;
    }
    return snap;
}

Ticker MatchingEngine::ticker(const std::string& symbol) const {
    Ticker t;
    t.symbol = symbol;
    auto lit = last_prices_.find(symbol);
    if (lit != last_prices_.end()) t.last_price = lit->second;

    auto bit = books_.find(symbol);
    if (bit != books_.end()) {
        if (!bit->second.bids().empty()) t.best_bid = bit->second.bids().begin()->first;
        if (!bit->second.asks().empty()) t.best_ask = bit->second.asks().begin()->first;
    }
    return t;
}

std::vector<OrderSnapshot> MatchingEngine::open_orders(const std::string& symbol) const {
    std::vector<OrderSnapshot> out;
    for (const auto& [id, o] : order_index_) {
        (void)id;
        if (!symbol.empty() && o->get_symbol() != symbol) continue;
        if (o->status != OrderStatus::New && o->status != OrderStatus::PartiallyFilled) continue;

        OrderSnapshot s;
        s.id = o->id;
        s.account_id = o->account_id;
        s.symbol = o->get_symbol();
        s.side = o->side;
        s.type = o->type;
        s.price = o->price;
        s.quantity = o->quantity;
        s.executed = o->executed;
        s.leaves = o->leaves;
        s.status = o->status;
        s.sequence = o->sequence;
        out.push_back(s);
    }
    return out;
}

} // namespace exchange

