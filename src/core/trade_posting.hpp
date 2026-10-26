#pragma once

#include "exchange/core/types.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace exchange {

struct Trade;

namespace detail {

// Average execution price of the fills produced by one incoming order. The
// notional is accumulated in 128-bit, so sum(price * quantity) cannot overflow.
Price average_price(const std::vector<Trade>& trades, Quantity executed);

// Stamps the engine-assigned ids (starting at first_id), the symbol and the
// shared timestamp onto every trade produced by one incoming order.
void stamp_trades(std::vector<Trade>& trades, std::string_view symbol, std::int64_t timestamp_ns,
                  TradeID first_id);

} // namespace detail
} // namespace exchange
