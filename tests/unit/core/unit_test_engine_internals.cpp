// Policy-level tests for the helpers extracted out of the engine hot path:
// request validation, the finalize decision table, and trade post-processing.
// These run without an engine instance so every branch is cheap to cover.
#include "exchange/core/engine.hpp"
#include "exchange/core/order.hpp"
#include "exchange/core/order_book.hpp"
#include "order_lifecycle.hpp"
#include "order_validation.hpp"
#include "trade_posting.hpp"

#include "support/test_util.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace exchange;

namespace {

Order make_order(Side side, OrderType type, TimeInForce tif, Price price, Quantity quantity, Quantity leaves,
                 Quantity executed, std::string_view symbol = "AAPL") {
    Order order;
    order.side = side;
    order.type = type;
    order.tif = tif;
    order.price = price;
    order.quantity = quantity;
    order.leaves = leaves;
    order.executed = executed;
    order.set_symbol(symbol);
    return order;
}

OrderRequest valid_request() {
    OrderRequest request;
    request.account_id = 1;
    request.symbol = "AAPL";
    request.side = Side::Buy;
    request.type = OrderType::Limit;
    request.price = 100;
    request.quantity = 10;
    return request;
}

void test_validation_accepts_valid_requests() {
    CHECK(!detail::validate_order_shape(valid_request()).has_value());

    OrderRequest market = valid_request();
    market.type = OrderType::Market;
    market.price = 0;
    CHECK(!detail::validate_order_shape(market).has_value());

    // Market orders never read the price, so a negative one is not rejected by
    // the shape check (the API layer forbids it before reaching the engine).
    market.price = -1;
    CHECK(!detail::validate_order_shape(market).has_value());
}

void test_validation_rejects_invalid_symbol() {
    OrderRequest request = valid_request();
    request.symbol.clear();
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidSymbol);

    request.symbol = "BAD$";
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidSymbol);

    request.symbol = std::string(kMaxSymbolLen, 'A');
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidSymbol);
}

void test_validation_rejects_invalid_numbers() {
    OrderRequest request = valid_request();
    request.account_id = 0;
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidAccount);

    request = valid_request();
    request.quantity = 0;
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidQuantity);

    request.quantity = -1;
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidQuantity);

    request = valid_request();
    request.price = 0;
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidPrice);

    request.price = -1;
    CHECK(detail::validate_order_shape(request) == detail::RejectReason::InvalidPrice);
}

void test_rejection_messages_are_stable() {
    CHECK(std::string(detail::rejection_message(detail::RejectReason::InvalidSymbol)) == "invalid symbol");
    CHECK(std::string(detail::rejection_message(detail::RejectReason::InvalidAccount)) ==
          "account_id must be non-zero");
    CHECK(std::string(detail::rejection_message(detail::RejectReason::InvalidQuantity)) ==
          "quantity must be positive");
    CHECK(std::string(detail::rejection_message(detail::RejectReason::InvalidPrice)) ==
          "limit price must be positive");
}

void test_filled_order_does_not_rest() {
    Order incoming = make_order(Side::Buy, OrderType::Limit, TimeInForce::GTC, 100, 10, 0, 10);
    OrderBook book;

    const detail::FinalizeDecision decision = detail::decide_finalize(incoming, book);
    CHECK(decision.status == OrderStatus::Filled);
    CHECK(decision.accepted);
    CHECK(!decision.rest);
    CHECK(decision.reason == nullptr);
}

void test_market_remainder_never_rests() {
    OrderBook book;

    Order unfilled = make_order(Side::Buy, OrderType::Market, TimeInForce::IOC, 0, 10, 10, 0);
    const detail::FinalizeDecision no_fill = detail::decide_finalize(unfilled, book);
    CHECK(no_fill.status == OrderStatus::Cancelled);
    CHECK(!no_fill.accepted);
    CHECK(!no_fill.rest);
    CHECK(std::string(no_fill.reason) == "market order: no further liquidity");

    Order partial = make_order(Side::Buy, OrderType::Market, TimeInForce::IOC, 0, 10, 5, 5);
    const detail::FinalizeDecision partially_filled = detail::decide_finalize(partial, book);
    CHECK(partially_filled.status == OrderStatus::PartiallyFilled);
    CHECK(partially_filled.accepted);
    CHECK(!partially_filled.rest);
    CHECK(std::string(partially_filled.reason) == "market order: no further liquidity");
}

void test_non_gtc_remainder_is_cancelled() {
    OrderBook book;

    Order ioc = make_order(Side::Buy, OrderType::Limit, TimeInForce::IOC, 100, 10, 10, 0);
    const detail::FinalizeDecision ioc_decision = detail::decide_finalize(ioc, book);
    CHECK(ioc_decision.status == OrderStatus::Cancelled);
    CHECK(!ioc_decision.accepted);
    CHECK(std::string(ioc_decision.reason) == "IOC: remainder cancelled");

    Order fok = make_order(Side::Buy, OrderType::Limit, TimeInForce::FOK, 100, 10, 10, 0);
    const detail::FinalizeDecision fok_decision = detail::decide_finalize(fok, book);
    CHECK(fok_decision.status == OrderStatus::Cancelled);
    CHECK(std::string(fok_decision.reason) == "FOK: remainder cancelled");
}

void test_gtc_remainder_rests_when_book_does_not_cross() {
    OrderBook book;

    Order incoming = make_order(Side::Buy, OrderType::Limit, TimeInForce::GTC, 100, 10, 10, 0);
    const detail::FinalizeDecision decision = detail::decide_finalize(incoming, book);
    CHECK(decision.status == OrderStatus::New);
    CHECK(decision.accepted);
    CHECK(decision.rest);
    CHECK(decision.reason == nullptr);
}

void test_partial_gtc_remainder_rests() {
    OrderBook book;

    Order incoming = make_order(Side::Buy, OrderType::Limit, TimeInForce::GTC, 100, 10, 5, 5);
    const detail::FinalizeDecision decision = detail::decide_finalize(incoming, book);
    CHECK(decision.status == OrderStatus::PartiallyFilled);
    CHECK(decision.accepted);
    CHECK(decision.rest);
}

void test_self_trade_prevention_blocks_resting() {
    OrderBook book;
    Order resting = make_order(Side::Sell, OrderType::Limit, TimeInForce::GTC, 100, 10, 10, 0, "AAPL");
    book.add(&resting);

    Order incoming = make_order(Side::Buy, OrderType::Limit, TimeInForce::GTC, 100, 10, 10, 0);
    const detail::FinalizeDecision decision = detail::decide_finalize(incoming, book);
    CHECK(decision.status == OrderStatus::Cancelled);
    CHECK(!decision.accepted);
    CHECK(!decision.rest);
    CHECK(std::string(decision.reason) ==
          "self-trade prevention: remainder would cross own resting liquidity");
}

void test_average_price_accumulates_notional() {
    std::vector<Trade> trades(2);
    trades[0].price = 100;
    trades[0].quantity = 2;
    trades[1].price = 200;
    trades[1].quantity = 1;

    CHECK(detail::average_price(trades, 3) == 133);
    CHECK(detail::average_price(trades, 0) == 0);
    CHECK(detail::average_price({}, 10) == 0);
}

void test_average_price_uses_128_bit_notional() {
    // price * quantity overflows int64, but the average still fits.
    constexpr Price kLargePrice = 5'000'000'000'000'000'000LL;
    std::vector<Trade> trades(1);
    trades[0].price = kLargePrice;
    trades[0].quantity = 2;

    CHECK(detail::average_price(trades, 2) == kLargePrice);
}

void test_stamp_trades_assigns_ids_symbol_and_timestamp() {
    std::vector<Trade> trades(3);
    detail::stamp_trades(trades, "AAPL", 42, 7);

    CHECK(trades[0].id == 7);
    CHECK(trades[1].id == 8);
    CHECK(trades[2].id == 9);
    CHECK(trades[0].symbol == "AAPL");
    CHECK(trades[2].symbol == "AAPL");
    CHECK(trades[0].timestamp_ns == 42);
    CHECK(trades[2].timestamp_ns == 42);
}

} // namespace

int main() {
    test_validation_accepts_valid_requests();
    test_validation_rejects_invalid_symbol();
    test_validation_rejects_invalid_numbers();
    test_rejection_messages_are_stable();
    test_filled_order_does_not_rest();
    test_market_remainder_never_rests();
    test_non_gtc_remainder_is_cancelled();
    test_gtc_remainder_rests_when_book_does_not_cross();
    test_partial_gtc_remainder_rests();
    test_self_trade_prevention_blocks_resting();
    test_average_price_accumulates_notional();
    test_average_price_uses_128_bit_notional();
    test_stamp_trades_assigns_ids_symbol_and_timestamp();
    return test::summary();
}
