#pragma once
#include <cstddef>
#include <cstdint>

// Quy ước Fixed-point: Giá $125.50 -> 12550 ticks (nhân hệ số 100)
using Price = uint32_t;
using Quantity = uint32_t;
using OrderId = uint64_t;

constexpr Price MIN_PRICE = 1;
constexpr Price MAX_PRICE = 100'000;  // Hỗ trợ dải giá từ $0.01 đến $1000.00
constexpr size_t MAX_ORDERS_PER_BOOK = 1'000'000;

enum class Side : uint8_t { BUY = 0, SELL = 1 };

// Node nằm trong Object Pool (Intrusive Linked List)
struct alignas(32) PoolOrder {
    OrderId order_id;
    uint64_t account_id;
    Price price;
    Quantity remaining_qty;
    Side side;

    // Con trỏ nội bộ dùng index thay vì raw pointer để tiết kiệm RAM và thân thiện với cache
    int32_t prev_idx{-1};
    int32_t next_idx{-1};
};

// Lệnh gửi từ Network/Client vào Engine
struct alignas(64) IngestOrderCommand {
    enum class Type : uint8_t { NEW, CANCEL } type;
    OrderId order_id;
    uint64_t account_id;
    Side side;
    Price price;
    Quantity quantity;
};

// Kết quả khớp lệnh
struct TradeEvent {
    OrderId maker_id;
    OrderId taker_id;
    Price match_price;
    Quantity match_qty;
};