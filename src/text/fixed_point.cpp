#include "exchange/text/fixed_point.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace exchange {
namespace {

constexpr bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

} // namespace

std::optional<Price> parse_fixed(std::string_view text) {
    if (text.empty())
        return std::nullopt;

    bool negative = false;
    if (text.front() == '-') {
        negative = true;
        text.remove_prefix(1);
    }
    if (text.empty())
        return std::nullopt;

    uint64_t whole = 0;
    size_t whole_digits = 0;
    while (!text.empty() && is_digit(text.front())) {
        const uint64_t digit = static_cast<uint64_t>(text.front() - '0');
        if (whole > (std::numeric_limits<uint64_t>::max() - digit) / 10)
            return std::nullopt;
        whole = whole * 10 + digit;
        ++whole_digits;
        text.remove_prefix(1);
    }

    if (whole_digits == 0)
        return std::nullopt;

    uint64_t fraction = 0;
    size_t fraction_digits = 0;
    bool has_decimal_point = false;
    if (!text.empty() && text.front() == '.') {
        has_decimal_point = true;
        text.remove_prefix(1);
        while (!text.empty() && is_digit(text.front())) {
            if (fraction_digits == static_cast<size_t>(kScaleDigits))
                return std::nullopt;
            fraction = fraction * 10 + static_cast<uint64_t>(text.front() - '0');
            ++fraction_digits;
            text.remove_prefix(1);
        }
    }

    if (!text.empty())
        return std::nullopt;
    if (has_decimal_point && fraction_digits == 0)
        return std::nullopt;

    while (fraction_digits < static_cast<size_t>(kScaleDigits)) {
        fraction *= 10;
        ++fraction_digits;
    }

    const uint64_t limit = static_cast<uint64_t>(std::numeric_limits<Price>::max());
    if (whole > limit / static_cast<uint64_t>(kScale))
        return std::nullopt;
    const uint64_t scaled = whole * static_cast<uint64_t>(kScale);
    if (scaled > limit - fraction)
        return std::nullopt;

    const Price value = static_cast<Price>(scaled + fraction);
    return negative ? -value : value;
}

std::string format_fixed(Price raw) {
    const bool negative = raw < 0;
    const uint64_t magnitude = negative ? static_cast<uint64_t>(-(raw + 1)) + 1 : static_cast<uint64_t>(raw);
    const uint64_t whole = magnitude / static_cast<uint64_t>(kScale);
    const uint64_t fraction = magnitude % static_cast<uint64_t>(kScale);

    std::string result = std::to_string(whole);
    if (fraction != 0) {
        std::string digits = std::to_string(fraction);
        digits.insert(0, static_cast<size_t>(kScaleDigits) - digits.size(), '0');
        while (!digits.empty() && digits.back() == '0')
            digits.pop_back();
        result += '.';
        result += digits;
    }
    if (negative)
        result.insert(result.begin(), '-');
    return result;
}

} // namespace exchange
