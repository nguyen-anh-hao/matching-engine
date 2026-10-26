#pragma once

#include <cstdint>
#include <optional>

namespace exchange {

struct OrderRequest;

namespace detail {

// Reasons an order request is rejected before it reaches the book. The list is
// exhaustive on purpose: rejection_message() maps every value to a stable
// client-visible string.
enum class RejectReason : std::uint8_t {
    InvalidSymbol,
    InvalidAccount,
    InvalidQuantity,
    InvalidPrice,
};

// Checks the request in isolation, without touching engine state. Capacity
// limits that depend on the book count stay in the engine.
std::optional<RejectReason> validate_order_shape(const OrderRequest& request);

const char* rejection_message(RejectReason reason) noexcept;

} // namespace detail
} // namespace exchange
