#include "api_helpers.hpp"

#include "fixed_point.hpp"
#include "json.hpp"

#include <charconv>
#include <system_error>

namespace exchange::api_detail {

std::string json_error(std::string_view code) {
    return "{\"error\":\"" + json::escape(code) + "\"}";
}

std::string error_body(int status) {
    switch (status) {
    case 400:
        return json_error("bad_request");
    case 404:
        return json_error("not_found");
    case 405:
        return json_error("method_not_allowed");
    case 408:
        return json_error("request_timeout");
    case 413:
        return json_error("payload_too_large");
    case 422:
        return json_error("rejected");
    case 431:
        return json_error("headers_too_large");
    case 503:
        return json_error("server_busy");
    default:
        return json_error("internal_error");
    }
}

std::string quote(std::string_view text) {
    return "\"" + json::escape(text) + "\"";
}

std::optional<uint64_t> parse_u64(std::string_view text) {
    uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}

std::optional<Side> parse_side(std::string_view text) {
    if (text == "BUY")
        return Side::Buy;
    if (text == "SELL")
        return Side::Sell;
    return std::nullopt;
}

std::optional<OrderType> parse_order_type(std::string_view text) {
    if (text == "LIMIT")
        return OrderType::Limit;
    if (text == "MARKET")
        return OrderType::Market;
    return std::nullopt;
}

std::optional<TimeInForce> parse_time_in_force(std::string_view text) {
    if (text == "GTC")
        return TimeInForce::GTC;
    if (text == "IOC")
        return TimeInForce::IOC;
    if (text == "FOK")
        return TimeInForce::FOK;
    return std::nullopt;
}

std::string trades_to_json(const std::vector<Trade>& trades) {
    std::string out = "[";
    for (size_t i = 0; i < trades.size(); ++i) {
        if (i != 0)
            out += ',';
        const Trade& trade = trades[i];
        out += "{\"trade_id\":" + std::to_string(trade.id);
        out += ",\"symbol\":" + quote(trade.symbol);
        out += ",\"price\":" + quote(format_fixed(trade.price));
        out += ",\"quantity\":" + quote(format_fixed(trade.quantity));
        out += ",\"maker_order_id\":" + std::to_string(trade.maker_order_id);
        out += ",\"taker_order_id\":" + std::to_string(trade.taker_order_id);
        out += ",\"maker_account_id\":" + std::to_string(trade.maker_account_id);
        out += ",\"taker_account_id\":" + std::to_string(trade.taker_account_id);
        out += ",\"timestamp_ns\":" + std::to_string(trade.timestamp_ns);
        out += '}';
    }
    out += ']';
    return out;
}

std::string orders_to_json(const std::vector<OrderSnapshot>& orders) {
    std::string out = "[";
    for (size_t i = 0; i < orders.size(); ++i) {
        if (i != 0)
            out += ',';
        const OrderSnapshot& order = orders[i];
        out += "{\"order_id\":" + std::to_string(order.id);
        out += ",\"account_id\":" + std::to_string(order.account_id);
        out += ",\"symbol\":" + quote(order.symbol);
        out += ",\"side\":\"" + std::string(to_string(order.side)) + "\"";
        out += ",\"type\":\"" + std::string(to_string(order.type)) + "\"";
        out += ",\"price\":" + quote(format_fixed(order.price));
        out += ",\"quantity\":" + quote(format_fixed(order.quantity));
        out += ",\"executed\":" + quote(format_fixed(order.executed));
        out += ",\"leaves\":" + quote(format_fixed(order.leaves));
        out += ",\"status\":\"" + std::string(to_string(order.status)) + "\"";
        out += ",\"sequence\":" + std::to_string(order.sequence);
        out += '}';
    }
    out += ']';
    return out;
}

std::string levels_to_json(const std::vector<PriceLevel>& levels) {
    std::string out = "[";
    for (size_t i = 0; i < levels.size(); ++i) {
        if (i != 0)
            out += ',';
        out +=
            "[" + quote(format_fixed(levels[i].price)) + "," + quote(format_fixed(levels[i].quantity)) + "]";
    }
    out += ']';
    return out;
}

} // namespace exchange::api_detail
