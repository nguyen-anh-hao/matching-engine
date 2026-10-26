#include "exchange/api/handler.hpp"

#include "exchange/api/serialization.hpp"
#include "exchange/text/fixed_point.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace exchange {
namespace {

constexpr uint32_t kMaxDepth = 1000;

} // namespace

void ApiHandler::open_orders(const http::Request& request, int& status, std::string& body) {
    std::string symbol;
    if (const auto query_symbol = http::query_param(request.query, "symbol"); query_symbol.has_value()) {
        if (!is_valid_symbol(*query_symbol)) {
            status = 400;
            body = api_detail::json_error("invalid_symbol");
            return;
        }
        symbol = *query_symbol;
    }

    std::vector<OrderSnapshot> orders;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        orders = engine_.open_orders(symbol);
    }
    body = "{\"orders\":" + api_detail::orders_to_json(orders) + "}";
}

void ApiHandler::order_book(const http::Request& request, int& status, std::string& body) {
    const auto symbol = http::query_param(request.query, "symbol");
    if (!symbol.has_value() || !is_valid_symbol(*symbol)) {
        status = 400;
        body = api_detail::json_error("invalid_symbol");
        return;
    }

    uint32_t depth = 0;
    const auto depth_text = http::query_param(request.query, "depth");
    if (depth_text.has_value() && !depth_text->empty()) {
        const auto parsed = api_detail::parse_u64(*depth_text);
        if (!parsed.has_value() || *parsed > kMaxDepth) {
            status = 400;
            body = api_detail::json_error("invalid_depth");
            return;
        }
        depth = static_cast<uint32_t>(*parsed);
    }

    OrderBookSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        snapshot = engine_.snapshot(*symbol, depth);
    }

    body = "{\"symbol\":" + api_detail::quote(*symbol);
    body += ",\"sequence\":" + std::to_string(snapshot.sequence);
    body += ",\"timestamp_ns\":" + std::to_string(snapshot.timestamp_ns);
    body += ",\"bids\":" + api_detail::levels_to_json(snapshot.bids);
    body += ",\"asks\":" + api_detail::levels_to_json(snapshot.asks) + "}";
}

void ApiHandler::ticker(const http::Request& request, int& status, std::string& body) {
    const auto symbol = http::query_param(request.query, "symbol");
    if (!symbol.has_value() || !is_valid_symbol(*symbol)) {
        status = 400;
        body = api_detail::json_error("invalid_symbol");
        return;
    }

    Ticker ticker_view;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        ticker_view = engine_.ticker(*symbol);
    }

    body = "{\"symbol\":" + api_detail::quote(*symbol);
    body += ",\"last_price\":" + api_detail::quote(format_fixed(ticker_view.last_price));
    body += ",\"best_bid\":" + api_detail::quote(format_fixed(ticker_view.best_bid));
    body += ",\"best_ask\":" + api_detail::quote(format_fixed(ticker_view.best_ask)) + "}";
}

void ApiHandler::prometheus(std::string& body, std::string& content_type) {
    size_t capacity = 0;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        capacity = engine_.order_pool_capacity();
    }
    body = metrics_.render(capacity);
    content_type = "text/plain; version=0.0.4; charset=utf-8";
}

} // namespace exchange
