// Tests for strict fixed-point parsing/formatting, symbol validation and the
// portable 128-bit notional accumulator.
#include "fixed_point.hpp"

#include "test_util.hpp"
#include "types.hpp"
#include "wide_math.hpp"

#include <cstdint>
#include <string>

using namespace exchange;

int main() {
    // ---- parse_fixed: valid inputs ----
    CHECK(parse_fixed("0") == 0);
    CHECK(parse_fixed("-0") == 0);
    CHECK(parse_fixed("1") == kScale);
    CHECK(parse_fixed("-1") == -kScale);
    CHECK(parse_fixed("0.00000001") == 1);
    CHECK(parse_fixed("0.1") == kScale / 10);
    CHECK(parse_fixed("123.456") == 12'345'600'000LL);
    CHECK(parse_fixed("-1.5") == -(kScale + kScale / 2));
    CHECK(parse_fixed("92233720368.54775807") == INT64_MAX);

    // ---- parse_fixed: malformed or out-of-range inputs ----
    CHECK(!parse_fixed("").has_value());
    CHECK(!parse_fixed("-").has_value());
    CHECK(!parse_fixed(".").has_value());
    CHECK(!parse_fixed(".5").has_value()); // integer part required
    CHECK(!parse_fixed("1.").has_value()); // empty fraction
    CHECK(!parse_fixed(" 1").has_value());
    CHECK(!parse_fixed("1 ").has_value());
    CHECK(!parse_fixed("+1").has_value());
    CHECK(!parse_fixed("1e5").has_value());
    CHECK(!parse_fixed("abc").has_value());
    CHECK(!parse_fixed("1.2.3").has_value());
    CHECK(!parse_fixed("1.123456789").has_value()); // more than kScaleDigits
    CHECK(!parse_fixed("92233720368.54775808").has_value());
    CHECK(!parse_fixed("92233720369").has_value());
    CHECK(!parse_fixed("99999999999999999999").has_value());

    // ---- format_fixed ----
    CHECK(format_fixed(0) == "0");
    CHECK(format_fixed(kScale) == "1");
    CHECK(format_fixed(-kScale) == "-1");
    CHECK(format_fixed(1) == "0.00000001");
    CHECK(format_fixed(-1) == "-0.00000001");
    CHECK(format_fixed(12'345'600'000LL) == "123.456");
    CHECK(format_fixed(-(kScale / 2)) == "-0.5");

    // ---- round trip ----
    for (const Price value :
         {Price{0}, Price{1}, Price{-1}, Price{kScale}, Price{-12'345'678'900LL}, Price{INT64_MAX}}) {
        const auto parsed = parse_fixed(format_fixed(value));
        CHECK(parsed.has_value());
        CHECK(parsed.value() == value);
    }

    // ---- symbol validation ----
    CHECK(is_valid_symbol("AAPL"));
    CHECK(is_valid_symbol("BRK.B"));
    CHECK(is_valid_symbol("BTC/USD"));
    CHECK(is_valid_symbol("a_b-c1"));
    CHECK(is_valid_symbol(std::string(kMaxSymbolChars, 'A')));
    CHECK(!is_valid_symbol(""));
    CHECK(!is_valid_symbol(std::string(kMaxSymbolChars + 1, 'A')));
    CHECK(!is_valid_symbol("A B"));
    CHECK(!is_valid_symbol("A$"));
    CHECK(!is_valid_symbol("AAPL;"));

    // ---- 128-bit notional arithmetic ----
    // A single int64 product would overflow here: 1e14 * 1e14 = 1e28.
    const Price large_price = 1'000'000 * kScale;
    const Quantity large_quantity = 1'000'000 * kScale;

    NotionalSum sum;
    sum.add(large_price, large_quantity);
    CHECK(sum.divide_by(large_quantity) == large_price);

    sum.add(large_price, large_quantity);
    CHECK(sum.divide_by(2 * large_quantity) == large_price);

    NotionalSum average;
    average.add(10 * kScale, kScale); // 10.0 x 1.0
    average.add(11 * kScale, kScale); // 11.0 x 1.0
    CHECK(average.divide_by(2 * kScale) == 10 * kScale + kScale / 2);
    CHECK(average.divide_by(0) == 0);

    return test::summary();
}
