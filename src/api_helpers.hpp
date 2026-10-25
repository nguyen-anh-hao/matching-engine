#pragma once

#include "order_book.hpp"
#include "types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace exchange::api_detail {

std::string json_error(std::string_view code);
std::string error_body(int status);
std::string quote(std::string_view text);

std::optional<uint64_t> parse_u64(std::string_view text);
std::optional<Side> parse_side(std::string_view text);
std::optional<OrderType> parse_order_type(std::string_view text);
std::optional<TimeInForce> parse_time_in_force(std::string_view text);

std::string trades_to_json(const std::vector<Trade>& trades);
std::string orders_to_json(const std::vector<OrderSnapshot>& orders);
std::string levels_to_json(const std::vector<PriceLevel>& levels);

} // namespace exchange::api_detail
