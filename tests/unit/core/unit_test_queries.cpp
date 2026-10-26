// Read-only projections: per-symbol isolation, L2 depth, ticker, deterministic
// open-order listing and order-pool reuse.
#include "exchange/core/engine.hpp"

#include "support/order_factory.hpp"
#include "support/test_util.hpp"

#include <cstdint>

using namespace exchange;
using namespace test;

namespace {

void test_multi_symbol_isolation() {
    MatchingEngine engine;
    engine.place_order(limit_order("AAA", Side::Buy, 5, 10, 1));
    engine.place_order(limit_order("BBB", Side::Sell, 7, 20, 2));

    const auto aaa = engine.snapshot("AAA", 0);
    CHECK(aaa.bids.size() == 1);
    CHECK(aaa.asks.empty());
    CHECK(aaa.bids[0].price == price(5));

    const auto bbb = engine.snapshot("BBB", 0);
    CHECK(bbb.asks.size() == 1);
    CHECK(bbb.bids.empty());
    CHECK(bbb.asks[0].quantity == qty(20));

    CHECK(engine.open_orders("AAA").size() == 1);
    CHECK(engine.open_orders("BBB").size() == 1);
    CHECK(engine.open_orders("").size() == 2);
}

void test_snapshot_depth_and_ticker() {
    MatchingEngine engine;
    for (int64_t level = 0; level < 3; ++level) {
        engine.place_order(limit_order("DEPTH", Side::Buy, 10 - level, 1, 1));
        engine.place_order(limit_order("DEPTH", Side::Sell, 11 + level, 1, 2));
    }

    const auto depth_two = engine.snapshot("DEPTH", 2);
    CHECK(depth_two.bids.size() == 2);
    CHECK(depth_two.asks.size() == 2);
    CHECK(depth_two.bids[0].price == price(10));
    CHECK(depth_two.bids[1].price == price(9));
    CHECK(depth_two.asks[0].price == price(11));
    CHECK(depth_two.asks[1].price == price(12));
    CHECK(depth_two.sequence == 6);

    const auto full = engine.snapshot("DEPTH", 0);
    CHECK(full.bids.size() == 3);
    CHECK(full.asks.size() == 3);

    const auto ticker = engine.ticker("DEPTH");
    CHECK(ticker.best_bid == price(10));
    CHECK(ticker.best_ask == price(11));
    CHECK(ticker.last_price == 0);
    CHECK(engine.ticker("UNKNOWN").best_bid == 0);
}

void test_open_orders_are_sorted_and_filtered() {
    MatchingEngine engine;
    const auto first = engine.place_order(limit_order("SORT", Side::Buy, 5, 1));
    const auto second = engine.place_order(limit_order("SORT", Side::Buy, 4, 1));
    const auto other = engine.place_order(limit_order("OTHER", Side::Buy, 3, 1));

    const auto orders = engine.open_orders("SORT");
    CHECK(orders.size() == 2);
    CHECK(orders[0].id == first.order_id);
    CHECK(orders[1].id == second.order_id);
    CHECK(orders[0].sequence < orders[1].sequence);

    const auto other_orders = engine.open_orders("OTHER");
    CHECK(other_orders.size() == 1);
    CHECK(other_orders[0].id == other.order_id);
}

void test_order_pool_is_reused() {
    MatchingEngine engine;
    for (int i = 0; i < 200; ++i) {
        const auto order = engine.place_order(limit_order("POOL", Side::Buy, i + 1, 1));
        CHECK(order.accepted);
        CHECK(engine.cancel_order(order.order_id, "POOL"));
    }
    CHECK(engine.order_pool_capacity() == 1);
}

} // namespace

int main() {
    test_multi_symbol_isolation();
    test_snapshot_depth_and_ticker();
    test_open_orders_are_sorted_and_filtered();
    test_order_pool_is_reused();
    return test::summary();
}
