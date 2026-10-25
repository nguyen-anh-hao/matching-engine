#pragma once

// Shared request builders for the order-book test suites.

#include "engine.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace test {

inline exchange::Price price(int64_t units) {
    return units * exchange::kScale;
}
inline exchange::Quantity qty(int64_t units) {
    return units * exchange::kScale;
}

inline exchange::OrderRequest limit_order(std::string symbol, exchange::Side side, int64_t price_units,
                                          int64_t qty_units, exchange::AccountID account = 1,
                                          exchange::TimeInForce tif = exchange::TimeInForce::GTC) {
    exchange::OrderRequest request;
    request.symbol = std::move(symbol);
    request.side = side;
    request.type = exchange::OrderType::Limit;
    request.price = price(price_units);
    request.quantity = qty(qty_units);
    request.account_id = account;
    request.tif = tif;
    return request;
}

inline exchange::OrderRequest market_order(std::string symbol, exchange::Side side, int64_t qty_units,
                                           exchange::AccountID account = 1) {
    exchange::OrderRequest request;
    request.symbol = std::move(symbol);
    request.side = side;
    request.type = exchange::OrderType::Market;
    request.quantity = qty(qty_units);
    request.account_id = account;
    request.tif = exchange::TimeInForce::IOC;
    return request;
}

} // namespace test
