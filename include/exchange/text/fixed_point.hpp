#pragma once

#include "exchange/core/types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace exchange {

// Strict decimal parser: "(+|-)123[.12345678]" -> raw int64 scaled by kScale.
// Rejects empty input, a missing integer part, trailing garbage, more than
// kScaleDigits fractional digits and values outside the int64 range. Parsing
// digits directly keeps the conversion exact; no floating point is involved.
std::optional<Price> parse_fixed(std::string_view text);

// Formats a raw fixed-point value as a decimal string with trailing zeros
// removed ("123.456", "0", "-0.00000001").
std::string format_fixed(Price raw);

} // namespace exchange
