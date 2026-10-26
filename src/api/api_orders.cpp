#include "exchange/api/handler.hpp"

#include "exchange/api/serialization.hpp"
#include "exchange/text/fixed_point.hpp"
#include "exchange/text/json.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace exchange {

void ApiHandler::place_order(const http::Request& request, int& status, std::string& body) {
    const auto bad_request = [&status, &body](std::string_view code) {
        status = 400;
        body = api_detail::json_error(code);
    };

    const auto fields = json::parse_object(request.body);
    if (!fields.has_value()) {
        bad_request("invalid_json");
        return;
    }

    const auto field = [&fields](std::string_view name) -> const std::string* {
        const auto it = fields->find(name);
        return it == fields->end() ? nullptr : &it->second;
    };

    const std::string* const symbol = field("symbol");
    if (symbol == nullptr || !is_valid_symbol(*symbol)) {
        bad_request("invalid_symbol");
        return;
    }

    const std::string* const side_text = field("side");
    const auto side = side_text == nullptr ? std::optional<Side>{} : api_detail::parse_side(*side_text);
    if (!side.has_value()) {
        bad_request("invalid_side");
        return;
    }

    auto type = OrderType::Limit;
    if (const std::string* const type_text = field("type"); type_text != nullptr) {
        const auto parsed = api_detail::parse_order_type(*type_text);
        if (!parsed.has_value()) {
            bad_request("invalid_type");
            return;
        }
        type = *parsed;
    }

    auto tif = TimeInForce::GTC;
    if (const std::string* const tif_text = field("time_in_force"); tif_text != nullptr) {
        const auto parsed = api_detail::parse_time_in_force(*tif_text);
        if (!parsed.has_value()) {
            bad_request("invalid_time_in_force");
            return;
        }
        tif = *parsed;
    }

    const std::string* const quantity_text = field("quantity");
    const auto quantity = quantity_text == nullptr ? std::optional<Price>{} : parse_fixed(*quantity_text);
    if (!quantity.has_value() || *quantity <= 0) {
        bad_request("invalid_quantity");
        return;
    }

    Price price = 0;
    if (const std::string* const price_text = field("price"); price_text != nullptr) {
        const auto parsed = parse_fixed(*price_text);
        if (!parsed.has_value() || *parsed <= 0) {
            bad_request("invalid_price");
            return;
        }
        price = *parsed;
    } else if (type == OrderType::Limit) {
        bad_request("missing_price");
        return;
    }

    const std::string* const account_text = field("account_id");
    const auto account_id =
        account_text == nullptr ? std::optional<uint64_t>{} : api_detail::parse_u64(*account_text);
    if (!account_id.has_value() || *account_id == 0) {
        bad_request("invalid_account_id");
        return;
    }

    OrderRequest order;
    order.symbol = *symbol;
    order.side = *side;
    order.type = type;
    order.tif = tif;
    order.price = price;
    order.quantity = *quantity;
    order.account_id = *account_id;

    const auto started = std::chrono::steady_clock::now();
    PlaceResult result;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        result = engine_.place_order(order);
    }
    const auto elapsed_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
            .count());

    metrics_.record_order();
    metrics_.record_trades(result.trades.size());
    if (!result.accepted)
        metrics_.record_rejection();
    metrics_.record_latency(elapsed_ns);

    status = (result.status == OrderStatus::Rejected) ? 422 : 200;
    body = "{\"accepted\":" + std::string(result.accepted ? "true" : "false");
    body += ",\"order_id\":" + std::to_string(result.order_id);
    body += ",\"status\":\"" + std::string(to_string(result.status)) + "\"";
    body += ",\"executed\":" + api_detail::quote(format_fixed(result.executed));
    body += ",\"leaves\":" + api_detail::quote(format_fixed(result.leaves));
    body += ",\"average_price\":" + api_detail::quote(format_fixed(result.average_price));
    if (!result.reason.empty())
        body += ",\"reason\":" + api_detail::quote(result.reason);
    body += ",\"trades\":" + api_detail::trades_to_json(result.trades) + "}";
}

void ApiHandler::cancel_order(const http::Request& request, int& status, std::string& body) {
    const auto id_text = http::query_param(request.query, "id");
    const auto symbol = http::query_param(request.query, "symbol");
    if (!id_text.has_value() || !symbol.has_value() || !is_valid_symbol(*symbol)) {
        status = 400;
        body = api_detail::json_error("invalid_cancel_parameters");
        return;
    }

    const auto order_id = api_detail::parse_u64(*id_text);
    if (!order_id.has_value()) {
        status = 400;
        body = api_detail::json_error("invalid_order_id");
        return;
    }

    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        cancelled = engine_.cancel_order(*order_id, *symbol);
    }

    if (cancelled) {
        metrics_.record_cancel();
        body = R"({"success":true})";
        return;
    }

    metrics_.record_rejection();
    status = 404;
    body = R"({"success":false,"error":"order_not_found"})";
}

} // namespace exchange
