// Immediate order semantics: FOK, IOC and market orders (full, partial and no
// liquidity), including the terminal status rules.
#include "engine.hpp"

#include "order_factory.hpp"
#include "test_util.hpp"

#include <cstdint>

using namespace exchange;
using namespace test;

namespace {

void test_fok_insufficient_liquidity() {
    MatchingEngine engine;
    engine.place_order(limit_order("AMD", Side::Sell, 12, 50, 2));

    const auto result = engine.place_order(limit_order("AMD", Side::Buy, 12, 100, 1, TimeInForce::FOK));
    CHECK(result.status == OrderStatus::Cancelled);
    CHECK(!result.accepted);
    CHECK(result.trades.empty());
    CHECK(result.executed == 0);
    CHECK(!result.reason.empty());
}

void test_fok_full_fill_across_levels() {
    MatchingEngine engine;
    engine.place_order(limit_order("FOK", Side::Sell, 10, 50, 2));
    engine.place_order(limit_order("FOK", Side::Sell, 11, 50, 2));

    const auto result = engine.place_order(limit_order("FOK", Side::Buy, 11, 100, 1, TimeInForce::FOK));
    CHECK(result.accepted);
    CHECK(result.status == OrderStatus::Filled);
    CHECK(result.trades.size() == 2);
    CHECK(result.average_price == price(10) + kScale / 2);
    CHECK(engine.snapshot("FOK", 0).asks.empty());
}

void test_fok_excludes_own_liquidity() {
    MatchingEngine engine;
    engine.place_order(limit_order("OWN", Side::Sell, 10, 50, 1)); // same account as taker
    engine.place_order(limit_order("OWN", Side::Sell, 10, 50, 2));

    const auto result = engine.place_order(limit_order("OWN", Side::Buy, 10, 100, 1, TimeInForce::FOK));
    CHECK(!result.accepted);
    CHECK(result.status == OrderStatus::Cancelled);
    CHECK(result.trades.empty());

    const auto snapshot = engine.snapshot("OWN", 0);
    CHECK(snapshot.asks.size() == 1);
    CHECK(snapshot.asks[0].quantity == qty(100)); // both asks are untouched
}

void test_ioc_partial_cancels_remainder() {
    MatchingEngine engine;
    engine.place_order(limit_order("IOC", Side::Sell, 10, 50, 2));

    const auto result = engine.place_order(limit_order("IOC", Side::Buy, 10, 100, 1, TimeInForce::IOC));
    CHECK(result.accepted);
    CHECK(result.status == OrderStatus::PartiallyFilled);
    CHECK(result.executed == qty(50));
    CHECK(result.leaves == qty(50));
    CHECK(result.trades.size() == 1);
    CHECK(!result.reason.empty());
    CHECK(engine.snapshot("IOC", 0).bids.empty()); // remainder never rested
}

void test_market_order_without_liquidity() {
    MatchingEngine engine;
    const auto result = engine.place_order(market_order("MKT", Side::Buy, 100));
    CHECK(!result.accepted);
    CHECK(result.status == OrderStatus::Cancelled);
    CHECK(result.executed == 0);
    CHECK(result.trades.empty());
    CHECK(!result.reason.empty());
}

void test_market_order_partial_fill() {
    MatchingEngine engine;
    engine.place_order(limit_order("MKT", Side::Sell, 10, 50, 2));

    const auto result = engine.place_order(market_order("MKT", Side::Buy, 100, 1));
    CHECK(result.accepted);
    CHECK(result.status == OrderStatus::PartiallyFilled); // never "Rejected" with fills
    CHECK(result.executed == qty(50));
    CHECK(result.leaves == qty(50));
    CHECK(!result.reason.empty());
    CHECK(engine.snapshot("MKT", 0).bids.empty());

    const auto ticker = engine.ticker("MKT");
    CHECK(ticker.last_price == price(10));
    CHECK(ticker.best_ask == 0);
}

void test_market_order_full_fill() {
    MatchingEngine engine;
    engine.place_order(limit_order("NVDA", Side::Sell, 100, 100, 2));

    const auto result = engine.place_order(market_order("NVDA", Side::Buy, 40, 3));
    CHECK(result.status == OrderStatus::Filled);
    CHECK(result.trades.size() == 1);
    CHECK(result.trades[0].price == price(100));
    CHECK(result.average_price == price(100));
    CHECK(result.reason.empty());
}

} // namespace

int main() {
    test_fok_insufficient_liquidity();
    test_fok_full_fill_across_levels();
    test_fok_excludes_own_liquidity();
    test_ioc_partial_cancels_remainder();
    test_market_order_without_liquidity();
    test_market_order_partial_fill();
    test_market_order_full_fill();
    return test::summary();
}
