#pragma once

#include "exchange/core/types.hpp"

#include <cstddef>
#include <string_view>

namespace exchange {

struct Order {
    Order* prev = nullptr;
    Order* next = nullptr;

    OrderID id = 0;
    AccountID account_id = 0;
    char symbol[kMaxSymbolLen] = {};

    Price price = 0;
    Quantity quantity = 0;
    Quantity leaves = 0;
    Quantity executed = 0;

    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    TimeInForce tif = TimeInForce::GTC;
    OrderStatus status = OrderStatus::New;

    Sequence sequence = 0;

    void set_symbol(std::string_view value) {
        const size_t length = value.size() < kMaxSymbolChars ? value.size() : kMaxSymbolChars;
        for (size_t i = 0; i < length; ++i) {
            symbol[i] = value[i];
        }
        symbol[length] = '\0';
    }

    std::string_view symbol_view() const { return std::string_view(symbol); }

    bool matches_symbol(std::string_view other) const { return symbol_view() == other; }

    void reset() {
        prev = nullptr;
        next = nullptr;
        id = 0;
        account_id = 0;
        symbol[0] = '\0';
        price = 0;
        quantity = 0;
        leaves = 0;
        executed = 0;
        side = Side::Buy;
        type = OrderType::Limit;
        tif = TimeInForce::GTC;
        status = OrderStatus::New;
        sequence = 0;
    }
};

} // namespace exchange
