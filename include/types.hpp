#pragma once
#include <cstddef>
#include <cstdint>

using Price = uint32_t;
using Quantity = uint32_t;
using OrderId = uint64_t;

constexpr Price MIN_PRICE = 1;
constexpr Price MAX_PRICE = 100'000;
constexpr size_t MAX_ORDERS_PER_BOOK = 1'000'000;
constexpr size_t NETWORK_BATCH_SIZE = 64;
constexpr size_t WAL_BLOCK_SIZE = 4096;  // Đã đổi tên để tránh xung đột macro
constexpr uint16_t UDP_PORT = 9876;

enum class Side : uint8_t { BUY = 0, SELL = 1 };

#pragma pack(push, 1)
struct alignas(32) IngestOrderCommand {
    OrderId order_id;
    uint64_t account_id;
    Price price;
    Quantity quantity;
    Side side;
    uint8_t padding[7];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct alignas(32) TradeRecord {
    OrderId maker_id;
    OrderId taker_id;
    Price match_price;
    Quantity match_qty;
    uint64_t timestamp_ns;
};
#pragma pack(pop)

using TradeEvent = TradeRecord;

struct alignas(32) PoolOrder {
    OrderId order_id;
    uint64_t account_id;
    Price price;
    Quantity remaining_qty;
    Side side;
    int32_t prev_idx{-1};
    int32_t next_idx{-1};
};