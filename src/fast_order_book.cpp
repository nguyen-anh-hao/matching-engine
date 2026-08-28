#include "fast_order_book.hpp"

#include <algorithm>

FastOrderBook::FastOrderBook() {
    best_bid_ = 0;
    best_ask_ = MAX_PRICE;
}

void FastOrderBook::add_to_book(int32_t order_idx, Price price, Side side) {
    PriceLevel& level = (side == Side::BUY) ? bids_[price] : asks_[price];
    PoolOrder& order = pool_.get(order_idx);

    order.prev_idx = level.tail_idx;
    order.next_idx = -1;

    if (level.tail_idx != -1) {
        pool_.get(level.tail_idx).next_idx = order_idx;
    } else {
        level.head_idx = order_idx;
    }
    level.tail_idx = order_idx;
    level.total_volume += order.remaining_qty;
}

void FastOrderBook::remove_from_book(int32_t order_idx, Price price, Side side) {
    PriceLevel& level = (side == Side::BUY) ? bids_[price] : asks_[price];
    PoolOrder& order = pool_.get(order_idx);

    if (order.prev_idx != -1) {
        pool_.get(order.prev_idx).next_idx = order.next_idx;
    } else {
        level.head_idx = order.next_idx;
    }

    if (order.next_idx != -1) {
        pool_.get(order.next_idx).prev_idx = order.prev_idx;
    } else {
        level.tail_idx = order.prev_idx;
    }

    level.total_volume -= order.remaining_qty;
    pool_.deallocate(order_idx);
}

void FastOrderBook::match_buy(const IngestOrderCommand& cmd, std::vector<TradeEvent>& out_trades) {
    Quantity remaining = cmd.quantity;

    // Quét trực tiếp từ Best Ask lên
    while (remaining > 0 && best_ask_ <= cmd.price) {
        PriceLevel& level = asks_[best_ask_];
        int32_t curr_idx = level.head_idx;

        while (remaining > 0 && curr_idx != -1) {
            PoolOrder& maker = pool_.get(curr_idx);
            int32_t next_idx = maker.next_idx;

            Quantity match_qty = std::min(remaining, maker.remaining_qty);
            out_trades.push_back({maker.order_id, cmd.order_id, best_ask_, match_qty});

            remaining -= match_qty;
            maker.remaining_qty -= match_qty;

            if (maker.remaining_qty == 0) {
                remove_from_book(curr_idx, best_ask_, Side::SELL);
            }
            curr_idx = next_idx;
        }

        // Cập nhật Best Ask nếu tầng giá hiện tại đã khớp hết sạch
        if (level.head_idx == -1) {
            while (best_ask_ <= MAX_PRICE && asks_[best_ask_].head_idx == -1) {
                best_ask_++;
            }
        }
    }

    // Phần dư treo vào Sổ Mua
    if (remaining > 0) {
        int32_t new_idx = pool_.allocate();
        if (new_idx != -1) {
            PoolOrder& ord = pool_.get(new_idx);
            ord.order_id = cmd.order_id;
            ord.account_id = cmd.account_id;
            ord.price = cmd.price;
            ord.remaining_qty = remaining;
            ord.side = Side::BUY;

            add_to_book(new_idx, cmd.price, Side::BUY);
            if (cmd.price > best_bid_) {
                best_bid_ = cmd.price;
            }
        }
    }
}

void FastOrderBook::match_sell(const IngestOrderCommand& cmd, std::vector<TradeEvent>& out_trades) {
    Quantity remaining = cmd.quantity;

    // Quét trực tiếp từ Best Bid xuống
    while (remaining > 0 && best_bid_ >= cmd.price && best_bid_ > 0) {
        PriceLevel& level = bids_[best_bid_];
        int32_t curr_idx = level.head_idx;

        while (remaining > 0 && curr_idx != -1) {
            PoolOrder& maker = pool_.get(curr_idx);
            int32_t next_idx = maker.next_idx;

            Quantity match_qty = std::min(remaining, maker.remaining_qty);
            out_trades.push_back({maker.order_id, cmd.order_id, best_bid_, match_qty});

            remaining -= match_qty;
            maker.remaining_qty -= match_qty;

            if (maker.remaining_qty == 0) {
                remove_from_book(curr_idx, best_bid_, Side::BUY);
            }
            curr_idx = next_idx;
        }

        if (level.head_idx == -1) {
            while (best_bid_ > 0 && bids_[best_bid_].head_idx == -1) {
                best_bid_--;
            }
        }
    }

    if (remaining > 0) {
        int32_t new_idx = pool_.allocate();
        if (new_idx != -1) {
            PoolOrder& ord = pool_.get(new_idx);
            ord.order_id = cmd.order_id;
            ord.account_id = cmd.account_id;
            ord.price = cmd.price;
            ord.remaining_qty = remaining;
            ord.side = Side::SELL;

            add_to_book(new_idx, cmd.price, Side::SELL);
            if (cmd.price < best_ask_) {
                best_ask_ = cmd.price;
            }
        }
    }
}

void FastOrderBook::process_order(const IngestOrderCommand& cmd,
                                  std::vector<TradeEvent>& out_trades) {
    if (cmd.side == Side::BUY) {
        match_buy(cmd, out_trades);
    } else {
        match_sell(cmd, out_trades);
    }
}