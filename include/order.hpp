#pragma once

#include "types.hpp"

#include <string>

namespace exchange {

constexpr size_t kMaxSymbolLen = 24;

// Intrusive order. Orders are allocated from a pre-allocated pool to eliminate
// malloc/free on the matching hot path. The symbol is stored inline (fixed
// array) so the struct never performs dynamic allocation.
struct Order {
    // Intrusive doubly-linked list pointers (used by LimitLevel FIFO queue and
    // by the pool free-list when the order is deallocated).
    Order* prev = nullptr;
    Order* next = nullptr;

    OrderID id = 0;
    AccountID account_id = 0;
    char symbol[kMaxSymbolLen] = {};

    Price price = 0;         // Limit price (0 for MARKET orders)
    Quantity quantity = 0;   // Original quantity
    Quantity leaves = 0;     // Remaining open quantity
    Quantity executed = 0;   // Filled quantity

    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    TimeInForce tif = TimeInForce::GTC;
    OrderStatus status = OrderStatus::New;

    Sequence sequence = 0;   // Global sequence for price-time priority

    void set_symbol(const std::string& s) {
        size_t n = s.size() < kMaxSymbolLen ? s.size() : kMaxSymbolLen - 1;
        for (size_t i = 0; i < n; ++i) symbol[i] = s[i];
        symbol[n] = '\0';
    }

    std::string get_symbol() const { return std::string(symbol); }

    void reset() {
        prev = next = nullptr;
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
