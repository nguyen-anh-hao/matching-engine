#include "engine.hpp"

#include "order_lifecycle.hpp"
#include "order_validation.hpp"
#include "trade_posting.hpp"

#include <cassert>
#include <cstdint>
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

// Populates a pooled order from the request. Ids and sequence numbers are
// assigned by the caller so the engine keeps a single source of ordering.
Order* build_order(OrderPool& pool, const OrderRequest& request, OrderID id, Sequence sequence) {
    Order* const order = pool.acquire();
    order->id = id;
    order->account_id = request.account_id;
    order->set_symbol(request.symbol);
    order->price = request.price;
    order->quantity = request.quantity;
    order->leaves = request.quantity;
    order->side = request.side;
    order->type = request.type;
    order->tif = request.tif;
    order->sequence = sequence;
    return order;
}

} // namespace

PlaceResult MatchingEngine::place_order(const OrderRequest& request) {
    if (const auto reason = detail::validate_order_shape(request); reason.has_value()) {
        return rejection(detail::rejection_message(*reason));
    }

    auto book_it = books_.find(request.symbol);
    if (book_it == books_.end()) {
        if (books_.size() >= kMaxBookSymbols) {
            return rejection("symbol limit reached");
        }
        book_it = books_.try_emplace(request.symbol).first;
    }
    OrderBook& book = book_it->second;

    Order* const incoming = build_order(pool_, request, next_order_id(), ++sequence_);

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

    detail::stamp_trades(result.trades, request.symbol, timestamp_ns, next_trade_id_ + 1);
    next_trade_id_ += static_cast<TradeID>(result.trades.size());
    if (!result.trades.empty()) {
        last_prices_[request.symbol] = result.trades.back().price;
    }

    result.average_price = detail::average_price(result.trades, incoming->executed);
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
    const detail::FinalizeDecision decision = detail::decide_finalize(*incoming, book);

    incoming->status = decision.status;
    result.executed = incoming->executed;
    result.leaves = incoming->leaves;
    result.accepted = decision.accepted;
    result.status = decision.status;
    if (decision.reason != nullptr) {
        result.reason = decision.reason;
    }

    if (!decision.rest) {
        pool_.release(incoming);
        return;
    }

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

} // namespace exchange
