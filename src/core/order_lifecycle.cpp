#include "order_lifecycle.hpp"

#include "exchange/core/order.hpp"
#include "exchange/core/order_book.hpp"

namespace exchange::detail {
namespace {

constexpr bool has_executed(const Order& order) {
    return order.executed > 0;
}

} // namespace

// Priority order: a fully executed order is done, a market order never rests,
// non-GTC orders drop the remainder, and a remainder only rests when it cannot
// cross the opposite side (a cross here means self-trade prevention skipped
// liquidity).
FinalizeDecision decide_finalize(const Order& incoming, const OrderBook& book) {
    FinalizeDecision decision;
    decision.accepted = has_executed(incoming);

    if (incoming.leaves == 0) {
        decision.status = OrderStatus::Filled;
        decision.accepted = true;
        return decision;
    }

    if (incoming.type == OrderType::Market) {
        decision.status = has_executed(incoming) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
        decision.reason = "market order: no further liquidity";
        return decision;
    }

    if (incoming.tif != TimeInForce::GTC) {
        decision.status = has_executed(incoming) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
        decision.reason =
            (incoming.tif == TimeInForce::IOC) ? "IOC: remainder cancelled" : "FOK: remainder cancelled";
        return decision;
    }

    if (book.would_cross(&incoming)) {
        decision.status = has_executed(incoming) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
        decision.reason = "self-trade prevention: remainder would cross own resting liquidity";
        return decision;
    }

    decision.status = has_executed(incoming) ? OrderStatus::PartiallyFilled : OrderStatus::New;
    decision.accepted = true;
    decision.rest = true;
    return decision;
}

} // namespace exchange::detail
