#include "order_book.hpp"

#include <chrono>

static uint64_t get_current_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

OrderBook::OrderBook(uint32_t symbol_id) : symbol_id_(symbol_id) {}

std::vector<Trade> OrderBook::add_order(Order order, uint64_t& trade_id_counter) {
    std::vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.remaining_quantity > 0 && !asks_.empty()) {
            auto best_ask_it = asks_.begin();
            double best_ask_price = best_ask_it->first;

            if (order.price < best_ask_price) {
                break;
            }

            auto& order_list = best_ask_it->second;
            while (order.remaining_quantity > 0 && !order_list.empty()) {
                Order& maker_order = order_list.front();
                uint32_t match_qty =
                    std::min(order.remaining_quantity, maker_order.remaining_quantity);

                trades.push_back(Trade{++trade_id_counter, symbol_id_, maker_order.order_id,
                                       order.order_id,
                                       best_ask_price,
                                       match_qty, get_current_time_ns()});

                order.remaining_quantity -= match_qty;
                maker_order.remaining_quantity -= match_qty;

                if (maker_order.remaining_quantity == 0) {
                    order_list.pop_front();
                }
            }

            if (order_list.empty()) {
                asks_.erase(best_ask_it);
            }
        }

        if (order.remaining_quantity > 0) {
            bids_[order.price].push_back(order);
        }

    } else {
        while (order.remaining_quantity > 0 && !bids_.empty()) {
            auto best_bid_it = bids_.begin();
            double best_bid_price = best_bid_it->first;

            if (order.price > best_bid_price) {
                break;
            }

            auto& order_list = best_bid_it->second;
            while (order.remaining_quantity > 0 && !order_list.empty()) {
                Order& maker_order = order_list.front();
                uint32_t match_qty =
                    std::min(order.remaining_quantity, maker_order.remaining_quantity);

                trades.push_back(Trade{++trade_id_counter, symbol_id_, maker_order.order_id,
                                       order.order_id, best_bid_price, match_qty,
                                       get_current_time_ns()});

                order.remaining_quantity -= match_qty;
                maker_order.remaining_quantity -= match_qty;

                if (maker_order.remaining_quantity == 0) {
                    order_list.pop_front();
                }
            }

            if (order_list.empty()) {
                bids_.erase(best_bid_it);
            }
        }

        if (order.remaining_quantity > 0) {
            asks_[order.price].push_back(order);
        }
    }

    return trades;
}

bool OrderBook::cancel_order(uint64_t order_id, Side side, double price) {
    if (side == Side::BUY) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            for (auto list_it = it->second.begin(); list_it != it->second.end(); ++list_it) {
                if (list_it->order_id == order_id) {
                    it->second.erase(list_it);
                    if (it->second.empty())
                        bids_.erase(it);
                    return true;
                }
            }
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            for (auto list_it = it->second.begin(); list_it != it->second.end(); ++list_it) {
                if (list_it->order_id == order_id) {
                    it->second.erase(list_it);
                    if (it->second.empty())
                        asks_.erase(it);
                    return true;
                }
            }
        }
    }
    return false;
}