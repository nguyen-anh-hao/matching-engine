#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace exchange {

using Price = int64_t;
using Quantity = int64_t;
using OrderID = uint64_t;
using AccountID = uint64_t;
using TradeID = uint64_t;
using Sequence = uint64_t;

constexpr int kScaleDigits = 8;
constexpr int64_t kScale = 100'000'000LL;

constexpr size_t kMaxSymbolLen = 24;
constexpr size_t kMaxSymbolChars = kMaxSymbolLen - 1;

enum class Side : uint8_t { Buy, Sell };
enum class OrderType : uint8_t { Limit, Market };
enum class TimeInForce : uint8_t { GTC, IOC, FOK };
enum class OrderStatus : uint8_t { New, PartiallyFilled, Filled, Cancelled, Rejected };

constexpr bool is_symbol_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '-' || c == '_' || c == '/';
}

constexpr bool is_valid_symbol(std::string_view symbol) {
    if (symbol.empty() || symbol.size() > kMaxSymbolChars)
        return false;
    for (const char c : symbol) {
        if (!is_symbol_char(c))
            return false;
    }
    return true;
}

inline const char* to_string(Side s) {
    switch (s) {
    case Side::Buy:
        return "BUY";
    case Side::Sell:
        return "SELL";
    }
    return "UNKNOWN";
}

inline const char* to_string(OrderType t) {
    switch (t) {
    case OrderType::Limit:
        return "LIMIT";
    case OrderType::Market:
        return "MARKET";
    }
    return "UNKNOWN";
}

inline const char* to_string(TimeInForce t) {
    switch (t) {
    case TimeInForce::GTC:
        return "GTC";
    case TimeInForce::IOC:
        return "IOC";
    case TimeInForce::FOK:
        return "FOK";
    }
    return "UNKNOWN";
}

inline const char* to_string(OrderStatus s) {
    switch (s) {
    case OrderStatus::New:
        return "NEW";
    case OrderStatus::PartiallyFilled:
        return "PARTIALLY_FILLED";
    case OrderStatus::Filled:
        return "FILLED";
    case OrderStatus::Cancelled:
        return "CANCELLED";
    case OrderStatus::Rejected:
        return "REJECTED";
    }
    return "UNKNOWN";
}

inline int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace exchange
