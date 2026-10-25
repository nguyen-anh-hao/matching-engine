#include "engine.hpp"

#include "wide_math.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace exchange {
namespace {

PlaceResult rejection(std::string reason) {
    PlaceResult result;
    result.status = OrderStatus::Rejected;
    result.reason = std::move(reason);
    return result;
}

// Average execution price of the incoming order. All trades in the vector were
// produced by this order, so the notional is simply the sum of all fills.
Price average_price(const std::vector<Trade>& trades, Quantity executed) {
    if (executed <= 0)
        return 0;
    NotionalSum notional;
    for (const Trade& trade : trades) {
        notional.add(trade.price, trade.quantity);
    }
    return notional.divide_by(executed);
}

} // namespace

PlaceResult MatchingEngine::place_order(const OrderRequest& request) {
    if (!is_valid_symbol(request.symbol)) {
        return rejection("invalid symbol");
    }
    if (request.account_id == 0) {
        return rejection("account_id must be non-zero");
    }
    if (request.quantity <= 0) {
        return rejection("quantity must be positive");
    }
    if (request.type == OrderType::Limit && request.price <= 0) {
        return rejection("limit price must be positive");
    }

    auto book_it = books_.find(request.symbol);
    if (book_it == books_.end()) {
        if (books_.size() >= kMaxBookSymbols) {
            return rejection("symbol limit reached");
        }
        book_it = books_.try_emplace(request.symbol).first;
    }
    OrderBook& book = book_it->second;

    Order* const incoming = pool_.acquire();
    incoming->id = next_order_id();
    incoming->account_id = request.account_id;
    incoming->set_symbol(request.symbol);
    incoming->price = request.price;
    incoming->quantity = request.quantity;
    incoming->leaves = request.quantity;
    incoming->side = request.side;
    incoming->type = request.type;
    incoming->tif = request.tif;
    incoming->sequence = ++sequence_;

    PlaceResult result;
    result.order_id = incoming->id;

    if (request.tif == TimeInForce::FOK && !book.can_fill_completely(incoming)) {
        pool_.release(incoming);
        result.status = OrderStatus::Cancelled;
        result.reason = "FOK: insufficient liquidity for a complete fill";
        return result;
    }

    const int64_t timestamp_ns = now_ns();
    book.match(incoming, result.trades);

    for (Trade& trade : result.trades) {
        trade.id = next_trade_id();
        trade.symbol = request.symbol;
        trade.timestamp_ns = timestamp_ns;
    }
    if (!result.trades.empty()) {
        last_prices_[request.symbol] = result.trades.back().price;
    }

    result.average_price = average_price(result.trades, incoming->executed);
    release_filled_makers(result.trades);
    finalize_order(incoming, book, result);
    return result;
}

void MatchingEngine::release_filled_makers(const std::vector<Trade>& trades) {
    for (const Trade& trade : trades) {
        const auto index_it = order_index_.find(trade.maker_order_id);
        if (index_it == order_index_.end())
            continue;

        Order* const maker = index_it->second;
        if (maker->status != OrderStatus::Filled)
            continue;

        order_index_.erase(index_it);
        pool_.release(maker);
    }
}

void MatchingEngine::finalize_order(Order* incoming, OrderBook& book, PlaceResult& result) {
    result.executed = incoming->executed;
    result.leaves = incoming->leaves;
    result.accepted = incoming->executed > 0;

    if (incoming->leaves == 0) {
        incoming->status = OrderStatus::Filled;
        result.accepted = true;
        result.status = OrderStatus::Filled;
        pool_.release(incoming);
        return;
    }

    if (incoming->type == OrderType::Market) {
        incoming->status = (incoming->executed > 0) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
        result.status = incoming->status;
        result.reason = "market order: no further liquidity";
        pool_.release(incoming);
        return;
    }

    if (incoming->tif != TimeInForce::GTC) {
        incoming->status = (incoming->executed > 0) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
        result.status = incoming->status;
        result.reason =
            (incoming->tif == TimeInForce::IOC) ? "IOC: remainder cancelled" : "FOK: remainder cancelled";
        pool_.release(incoming);
        return;
    }

    if (book.would_cross(incoming)) {
        incoming->status = (incoming->executed > 0) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
        result.status = incoming->status;
        result.reason = "self-trade prevention: remainder would cross own resting liquidity";
        pool_.release(incoming);
        return;
    }

    incoming->status = (incoming->executed > 0) ? OrderStatus::PartiallyFilled : OrderStatus::New;
    result.accepted = true;
    result.status = incoming->status;

    book.add(incoming);
    order_index_.emplace(incoming->id, incoming);
    assert(!book.crossed());
}

bool MatchingEngine::cancel_order(OrderID order_id, const std::string& symbol) {
    const auto index_it = order_index_.find(order_id);
    if (index_it == order_index_.end())
        return false;

    Order* const order = index_it->second;
    if (!order->matches_symbol(symbol))
        return false;

    const auto book_it = books_.find(symbol);
    if (book_it == books_.end())
        return false;
    if (!book_it->second.remove(order))
        return false;

    order_index_.erase(index_it);
    order->status = OrderStatus::Cancelled;
    pool_.release(order);
    return true;
}

OrderBookSnapshot MatchingEngine::snapshot(const std::string& symbol, uint32_t depth) const {
    OrderBookSnapshot snapshot;
    snapshot.symbol = symbol;
    snapshot.sequence = sequence_;
    snapshot.timestamp_ns = now_ns();

    const auto book_it = books_.find(symbol);
    if (book_it == books_.end())
        return snapshot;

    const auto append_levels = [depth](const auto& levels, std::vector<PriceLevel>& out) {
        for (const auto& [price, level] : levels) {
            if (depth != 0 && out.size() >= depth)
                break;
            out.push_back(PriceLevel{price, level.total_qty(), level.count()});
        }
    };
    append_levels(book_it->second.bids(), snapshot.bids);
    append_levels(book_it->second.asks(), snapshot.asks);
    return snapshot;
}

Ticker MatchingEngine::ticker(const std::string& symbol) const {
    Ticker ticker;
    ticker.symbol = symbol;

    const auto price_it = last_prices_.find(symbol);
    if (price_it != last_prices_.end())
        ticker.last_price = price_it->second;

    const auto book_it = books_.find(symbol);
    if (book_it != books_.end()) {
        const OrderBook& book = book_it->second;
        if (!book.bids().empty())
            ticker.best_bid = book.bids().begin()->first;
        if (!book.asks().empty())
            ticker.best_ask = book.asks().begin()->first;
    }
    return ticker;
}

std::vector<OrderSnapshot> MatchingEngine::open_orders(const std::string& symbol) const {
    std::vector<OrderSnapshot> orders;
    for (const auto& entry : order_index_) {
        const Order* const order = entry.second;
        if (!symbol.empty() && !order->matches_symbol(symbol))
            continue;
        if (order->status != OrderStatus::New && order->status != OrderStatus::PartiallyFilled) {
            continue;
        }

        OrderSnapshot snapshot;
        snapshot.id = order->id;
        snapshot.account_id = order->account_id;
        snapshot.symbol = std::string(order->symbol_view());
        snapshot.side = order->side;
        snapshot.type = order->type;
        snapshot.price = order->price;
        snapshot.quantity = order->quantity;
        snapshot.executed = order->executed;
        snapshot.leaves = order->leaves;
        snapshot.status = order->status;
        snapshot.sequence = order->sequence;
        orders.push_back(std::move(snapshot));
    }

    std::sort(orders.begin(), orders.end(),
              [](const OrderSnapshot& lhs, const OrderSnapshot& rhs) { return lhs.sequence < rhs.sequence; });
    return orders;
}

} // namespace exchange
