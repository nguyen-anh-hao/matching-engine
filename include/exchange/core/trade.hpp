#pragma once

#include "exchange/core/types.hpp"

#include <cstdint>
#include <string>

namespace exchange {

// A single executed trade between a maker (resting order) and a taker (incoming).
struct Trade {
    TradeID id = 0;
    std::string symbol;
    Price price = 0;
    Quantity quantity = 0;
    OrderID maker_order_id = 0;
    OrderID taker_order_id = 0;
    AccountID maker_account_id = 0;
    AccountID taker_account_id = 0;
    int64_t timestamp_ns = 0;
};

} // namespace exchange
