#include "engine.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace exchange {

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
