// Self-trade prevention: own resting liquidity is skipped, and any remainder
// that could only rest crossed against the same account is cancelled.
#include "engine.hpp"

#include "order_factory.hpp"
#include "test_util.hpp"

#include <cstdint>

using namespace exchange;
using namespace test;

namespace {

void test_self_trade_never_crosses_book() {
    MatchingEngine engine;
    engine.place_order(limit_order("STP", Side::Sell, 10, 100, 1));

    // Same account tries to cross its own ask: the remainder must be cancelled,
    // not rested at 10 (which would leave a locked book).
    const auto result = engine.place_order(limit_order("STP", Side::Buy, 10, 100, 1));
    CHECK(!result.accepted);
    CHECK(result.status == OrderStatus::Cancelled);
    CHECK(result.executed == 0);
    CHECK(result.trades.empty());
    CHECK(!result.reason.empty());

    const auto snapshot = engine.snapshot("STP", 0);
    CHECK(snapshot.bids.empty());
    CHECK(snapshot.asks.size() == 1);
    CHECK(snapshot.asks[0].quantity == qty(100));
}

void test_self_trade_rests_when_not_crossing() {
    MatchingEngine engine;
    engine.place_order(limit_order("STP", Side::Sell, 10, 100, 1));

    const auto result = engine.place_order(limit_order("STP", Side::Buy, 9, 100, 1));
    CHECK(result.accepted);
    CHECK(result.status == OrderStatus::New);

    const auto snapshot = engine.snapshot("STP", 0);
    CHECK(snapshot.bids.size() == 1);
    CHECK(snapshot.bids[0].price == price(9));
    CHECK(snapshot.asks.size() == 1);
    CHECK(snapshot.asks[0].price == price(10));
    CHECK(snapshot.bids[0].price < snapshot.asks[0].price);
}

void test_self_trade_partial_then_blocked() {
    MatchingEngine engine;
    engine.place_order(limit_order("STP", Side::Sell, 10, 50, 2));
    engine.place_order(limit_order("STP", Side::Sell, 10, 50, 1)); // own resting ask

    const auto result = engine.place_order(limit_order("STP", Side::Buy, 10, 150, 1));
    CHECK(result.accepted);
    CHECK(result.status == OrderStatus::PartiallyFilled);
    CHECK(result.executed == qty(50));
    CHECK(result.leaves == qty(100));
    CHECK(result.trades.size() == 1);
    CHECK(result.trades[0].maker_account_id == 2);

    const auto snapshot = engine.snapshot("STP", 0);
    CHECK(snapshot.bids.empty());
    CHECK(snapshot.asks.size() == 1);
    CHECK(snapshot.asks[0].quantity == qty(50));
}

} // namespace

int main() {
    test_self_trade_never_crosses_book();
    test_self_trade_rests_when_not_crossing();
    test_self_trade_partial_then_blocked();
    return test::summary();
}
