#pragma once

#include "exchange/core/types.hpp"

#include <cstdint>

namespace exchange {

struct Order;
class OrderBook;

namespace detail {

// Outcome of an incoming order after matching: whether it rests on the book,
// the status reported to the caller and the reason when the remainder is not
// rested. Produced by a pure function so the policy is table-testable without
// an engine instance.
struct FinalizeDecision {
    OrderStatus status = OrderStatus::New;
    bool accepted = false;
    bool rest = false;
    const char* reason = nullptr;
};

FinalizeDecision decide_finalize(const Order& incoming, const OrderBook& book);

} // namespace detail
} // namespace exchange
