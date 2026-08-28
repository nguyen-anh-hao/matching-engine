#pragma once
#include <cstdint>
#include <string>

enum class Side : char { BUY = 'B', SELL = 'S' };

enum class OrderType : char { LIMIT = 'L', MARKET = 'M' };

enum class OrderStatus { NEW, PARTIALLY_FILLED, FILLED, CANCELLED, REJECTED };

struct Order {
    uint64_t order_id;
    uint64_t account_id;
    uint32_t symbol_id;
    Side side;
    double price;
    uint32_t initial_quantity;
    uint32_t remaining_quantity;
    uint64_t timestamp_ns;
};

struct Trade {
    uint64_t trade_id;
    uint32_t symbol_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    double price;
    uint32_t quantity;
    uint64_t timestamp_ns;
};