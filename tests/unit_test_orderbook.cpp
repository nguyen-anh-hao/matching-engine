// Self-contained unit tests for the matching engine (Price-Time Priority,
// Partial Fill, Cancel, FOK, Market orders). No external test framework is
// required so the suite runs anywhere with a C++20 compiler.
#include "engine.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace exchange;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL [line %d]: %s\n", __LINE__, #cond);            \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static Price P(double v) { return static_cast<Price>(v * static_cast<double>(kScale)); }

static OrderRequest buy_limit(const std::string& sym, double px, double qty) {
    OrderRequest r;
    r.symbol = sym; r.side = Side::Buy; r.type = OrderType::Limit;
    r.price = P(px); r.quantity = P(qty); r.tif = TimeInForce::GTC;
    r.account_id = 1;
    return r;
}

static OrderRequest sell_limit(const std::string& sym, double px, double qty) {
    OrderRequest r;
    r.symbol = sym; r.side = Side::Sell; r.type = OrderType::Limit;
    r.price = P(px); r.quantity = P(qty); r.tif = TimeInForce::GTC;
    r.account_id = 2;
    return r;
}

int main() {
    // ---- 1. Price-time priority + FIFO within a level ----
    {
        MatchingEngine e;
        auto o1 = e.place_order(sell_limit("AAPL", 10.0, 100)); // ask 10 @ 100 (seq 1)
        auto o2 = e.place_order(sell_limit("AAPL", 10.0, 100)); // ask 10 @ 100 (seq 2)
        auto o3 = e.place_order(buy_limit("AAPL", 10.0, 150));  // cross, fills 150
        CHECK(o3.status == OrderStatus::Filled);
        CHECK(o3.trades.size() == 2);
        CHECK(o3.trades[0].maker_order_id == o1.order_id); // FIFO: seq1 filled first
        CHECK(o3.trades[1].maker_order_id == o2.order_id);
        CHECK(o3.trades[0].quantity == P(100));
        CHECK(o3.trades[1].quantity == P(50));
        auto open = e.open_orders("AAPL");
        CHECK(open.size() == 1);
        CHECK(open[0].id == o2.order_id);
        CHECK(open[0].leaves == P(50));
    }

    // ---- 2. Partial fill rests the remainder ----
    {
        MatchingEngine e;
        auto o1 = e.place_order(buy_limit("MSFT", 9.0, 100));
        auto o2 = e.place_order(sell_limit("MSFT", 9.0, 150));
        CHECK(o2.status == OrderStatus::PartiallyFilled);
        CHECK(o2.trades.size() == 1);
        CHECK(o2.trades[0].quantity == P(100));
        auto snap = e.snapshot("MSFT", 10);
        CHECK(snap.bids.empty());       // buy fully consumed
        CHECK(snap.asks.size() == 1);   // remaining 50 rests
        CHECK(snap.asks[0].quantity == P(50));
    }

    // ---- 3. Cancel an open order ----
    {
        MatchingEngine e;
        auto o1 = e.place_order(buy_limit("TSLA", 8.0, 100));
        CHECK(o1.accepted);
        bool ok = e.cancel_order(o1.order_id, "TSLA");
        CHECK(ok);
        auto snap = e.snapshot("TSLA", 10);
        CHECK(snap.bids.empty());
        CHECK(e.open_orders("TSLA").empty());
        bool ok2 = e.cancel_order(o1.order_id, "TSLA");
        CHECK(!ok2); // already gone
    }

    // ---- 4. FOK with insufficient liquidity is killed ----
    {
        MatchingEngine e;
        e.place_order(sell_limit("AMD", 12.0, 50));
        OrderRequest fok = buy_limit("AMD", 12.0, 100);
        fok.tif = TimeInForce::FOK;
        auto r = e.place_order(fok);
        CHECK(r.status == OrderStatus::Cancelled);
        CHECK(!r.accepted);
        CHECK(r.trades.empty());
    }

    // ---- 5. Market order fills against the resting book ----
    {
        MatchingEngine e;
        auto o1 = e.place_order(sell_limit("NVDA", 100.0, 100));
        OrderRequest mkt;
        mkt.symbol = "NVDA"; mkt.side = Side::Buy; mkt.type = OrderType::Market;
        mkt.quantity = P(40); mkt.tif = TimeInForce::IOC; mkt.account_id = 3;
        auto o2 = e.place_order(mkt);
        CHECK(o2.status == OrderStatus::Filled);
        CHECK(o2.trades.size() == 1);
        CHECK(o2.trades[0].price == P(100.0));
        CHECK(o2.average_price == P(100.0));
    }

    // ---- 6. Crossed spread fills at the resting price ----
    {
        MatchingEngine e;
        e.place_order(sell_limit("GOOG", 11.0, 100));
        auto r = e.place_order(buy_limit("GOOG", 12.0, 100));
        CHECK(r.status == OrderStatus::Filled);
        CHECK(r.trades.size() == 1);
        CHECK(r.trades[0].price == P(11.0)); // price improvement to the maker
    }

    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", failures);
    return 1;
}
