#include "order_validation.hpp"

#include "engine.hpp"

namespace exchange::detail {

std::optional<RejectReason> validate_order_shape(const OrderRequest& request) {
    if (!is_valid_symbol(request.symbol))
        return RejectReason::InvalidSymbol;
    if (request.account_id == 0)
        return RejectReason::InvalidAccount;
    if (request.quantity <= 0)
        return RejectReason::InvalidQuantity;
    if (request.type == OrderType::Limit && request.price <= 0)
        return RejectReason::InvalidPrice;
    return std::nullopt;
}

const char* rejection_message(RejectReason reason) noexcept {
    switch (reason) {
    case RejectReason::InvalidSymbol:
        return "invalid symbol";
    case RejectReason::InvalidAccount:
        return "account_id must be non-zero";
    case RejectReason::InvalidQuantity:
        return "quantity must be positive";
    case RejectReason::InvalidPrice:
        return "limit price must be positive";
    }
    return "invalid order";
}

} // namespace exchange::detail
