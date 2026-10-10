#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace exchange {

// Fixed-point scalars. All prices/quantities are expressed as int64 fixed-point
// values scaled by kScale (1e8). This avoids IEEE-754 floating point errors and
// makes price comparison a single integer comparison.
using Price    = int64_t;
using Quantity = int64_t;
using OrderID  = uint64_t;
using AccountID = uint64_t;
using TradeID  = uint64_t;
using Sequence = uint64_t;

constexpr int64_t kScale = 100'000'000LL; // 1e8

enum class Side : uint8_t { Buy, Sell };
enum class OrderType : uint8_t { Limit, Market };
enum class TimeInForce : uint8_t { GTC, IOC, FOK };
enum class OrderStatus : uint8_t { New, PartiallyFilled, Filled, Cancelled, Rejected };
enum class LiquidityRole : uint8_t { Maker, Taker };

inline const char* to_string(Side s) {
    switch (s) {
        case Side::Buy:  return "BUY";
        case Side::Sell: return "SELL";
    }
    return "UNKNOWN";
}

inline const char* to_string(OrderType t) {
    switch (t) {
        case OrderType::Limit:  return "LIMIT";
        case OrderType::Market: return "MARKET";
    }
    return "UNKNOWN";
}

inline const char* to_string(TimeInForce t) {
    switch (t) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
    }
    return "UNKNOWN";
}

inline const char* to_string(OrderStatus s) {
    switch (s) {
        case OrderStatus::New:             return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled:          return "FILLED";
        case OrderStatus::Cancelled:       return "CANCELLED";
        case OrderStatus::Rejected:        return "REJECTED";
    }
    return "UNKNOWN";
}

inline const char* to_string(LiquidityRole r) {
    switch (r) {
        case LiquidityRole::Maker: return "MAKER";
        case LiquidityRole::Taker: return "TAKER";
    }
    return "UNKNOWN";
}

// Convert fixed-point raw to human-readable double (logging/debug only).
inline double to_double(int64_t raw) {
    return static_cast<double>(raw) / static_cast<double>(kScale);
}

// Wall-clock timestamp in nanoseconds since the Unix epoch.
inline int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace exchange
