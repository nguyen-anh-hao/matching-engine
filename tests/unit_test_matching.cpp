// Core matching behaviour: price-time priority, partial fills, cancels, input
// validation and the read-only projections (book, ticker, open orders).
#include "engine.hpp"

#include "order_factory.hpp"
#include "test_util.hpp"

#include <cstdint>
#include <string>

using namespace exchange;
using namespace test;

namespace {

void test_price_time_priority() {
    MatchingEngine engine;
    const auto first = engine.place_order(limit_order("AAPL", Side::Sell, 10, 100, 2));
    const auto second = engine.place_order(limit_order("AAPL", Side::Sell, 10, 100, 2));
    const auto taker = engine.place_order(limit_order("AAPL", Side::Buy, 10, 150));

    CHECK(taker.accepted);
    CHECK(taker.status == OrderStatus::Filled);
    CHECK(taker.trades.size() == 2);
    CHECK(taker.trades[0].maker_order_id == first.order_id);
    CHECK(taker.trades[1].maker_order_id == second.order_id);
    CHECK(taker.trades[0].quantity == qty(100));
    CHECK(taker.trades[1].quantity == qty(50));
    CHECK(taker.trades[0].maker_account_id == 2);
    CHECK(taker.trades[0].taker_account_id == 1);
    CHECK(taker.trades[0].timestamp_ns > 0);
    CHECK(taker.trades[0].timestamp_ns == taker.trades[1].timestamp_ns);

    const auto open = engine.open_orders("AAPL");
    CHECK(open.size() == 1);
    CHECK(open[0].id == second.order_id);
    CHECK(open[0].leaves == qty(50));
}

void test_partial_fill_rests_remainder() {
    MatchingEngine engine;
    const auto maker = engine.place_order(limit_order("MSFT", Side::Buy, 9, 100, 2));
    const auto taker = engine.place_order(limit_order("MSFT", Side::Sell, 9, 150, 1));

    CHECK(maker.accepted);
    CHECK(taker.status == OrderStatus::PartiallyFilled);
    CHECK(taker.trades.size() == 1);
    CHECK(taker.trades[0].quantity == qty(100));
    CHECK(taker.executed == qty(100));
    CHECK(taker.leaves == qty(50));

    const auto snapshot = engine.snapshot("MSFT", 10);
    CHECK(snapshot.bids.empty());
    CHECK(snapshot.asks.size() == 1);
    CHECK(snapshot.asks[0].price == price(9));
    CHECK(snapshot.asks[0].quantity == qty(50));
}

void test_cancel_semantics() {
    MatchingEngine engine;
    const auto order = engine.place_order(limit_order("TSLA", Side::Buy, 8, 100));
    CHECK(order.accepted);

    CHECK(!engine.cancel_order(order.order_id, "OTHER"));
    CHECK(engine.cancel_order(order.order_id, "TSLA"));
    CHECK(!engine.cancel_order(order.order_id, "TSLA")); // already gone
    CHECK(!engine.cancel_order(999999, "TSLA"));
    CHECK(engine.snapshot("TSLA", 10).bids.empty());
    CHECK(engine.open_orders("TSLA").empty());
}

void test_invalid_requests_are_rejected() {
    MatchingEngine engine;

    const auto empty_symbol = engine.place_order(limit_order("", Side::Buy, 10, 1));
    CHECK(empty_symbol.status == OrderStatus::Rejected);
    CHECK(!empty_symbol.reason.empty());

    const auto long_symbol =
        engine.place_order(limit_order(std::string(kMaxSymbolChars + 1, 'A'), Side::Buy, 10, 1));
    CHECK(long_symbol.status == OrderStatus::Rejected);

    const auto bad_symbol = engine.place_order(limit_order("BAD$", Side::Buy, 10, 1));
    CHECK(bad_symbol.status == OrderStatus::Rejected);

    const auto zero_qty = engine.place_order(limit_order("AAPL", Side::Buy, 10, 0));
    CHECK(zero_qty.status == OrderStatus::Rejected);

    const auto zero_price = engine.place_order(limit_order("AAPL", Side::Buy, 0, 1));
    CHECK(zero_price.status == OrderStatus::Rejected);

    OrderRequest no_account = limit_order("AAPL", Side::Buy, 10, 1);
    no_account.account_id = 0;
    CHECK(engine.place_order(no_account).status == OrderStatus::Rejected);
}

void test_price_improvement() {
    MatchingEngine engine;
    engine.place_order(limit_order("GOOG", Side::Sell, 11, 100, 2));

    const auto result = engine.place_order(limit_order("GOOG", Side::Buy, 12, 100, 1));
    CHECK(result.status == OrderStatus::Filled);
    CHECK(result.trades.size() == 1);
    CHECK(result.trades[0].price == price(11)); // maker price wins
}

} // namespace

int main() {
    test_price_time_priority();
    test_partial_fill_rests_remainder();
    test_cancel_semantics();
    test_invalid_requests_are_rejected();
    test_price_improvement();
    return test::summary();
}
