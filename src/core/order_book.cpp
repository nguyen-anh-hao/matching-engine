#include "exchange/core/order_book.hpp"

#include <algorithm>

namespace exchange {

void OrderBook::add(Order* order) {
    if (order->side == Side::Buy) {
        bids_[order->price].append(order);
    } else {
        asks_[order->price].append(order);
    }
}

bool OrderBook::remove(Order* order) {
    if (order->side == Side::Buy) {
        const auto level_it = bids_.find(order->price);
        if (level_it == bids_.end())
            return false;
        level_it->second.remove(order);
        if (level_it->second.empty())
            bids_.erase(level_it);
        return true;
    }

    const auto level_it = asks_.find(order->price);
    if (level_it == asks_.end())
        return false;
    level_it->second.remove(order);
    if (level_it->second.empty())
        asks_.erase(level_it);
    return true;
}

void OrderBook::match(Order* incoming, std::vector<Trade>& trades) {
    const bool is_buy = (incoming->side == Side::Buy);

    const auto match_against = [&](auto& opposite) {
        auto level_it = opposite.begin();
        while (level_it != opposite.end() && incoming->leaves > 0) {
            if (incoming->type == OrderType::Limit) {
                const Price level_price = level_it->first;
                if (is_buy ? level_price > incoming->price : level_price < incoming->price)
                    break;
            }

            LimitLevel& level = level_it->second;
            Order* resting = level.head();
            while (resting != nullptr && incoming->leaves > 0) {
                Order* const next = resting->next;

                if (resting->account_id == incoming->account_id) {
                    resting = next;
                    continue;
                }

                const Quantity match_qty = std::min(incoming->leaves, resting->leaves);
                level.reduce(resting, match_qty);
                incoming->leaves -= match_qty;
                incoming->executed += match_qty;

                Trade trade;
                trade.price = resting->price;
                trade.quantity = match_qty;
                trade.maker_order_id = resting->id;
                trade.taker_order_id = incoming->id;
                trade.maker_account_id = resting->account_id;
                trade.taker_account_id = incoming->account_id;
                trades.push_back(trade);

                if (resting->leaves == 0) {
                    level.remove(resting);
                    resting->status = OrderStatus::Filled;
                } else {
                    resting->status = OrderStatus::PartiallyFilled;
                }
                resting = next;
            }

            if (level.empty()) {
                level_it = opposite.erase(level_it);
            } else {
                ++level_it;
            }
        }
    };

    if (is_buy) {
        match_against(asks_);
    } else {
        match_against(bids_);
    }
}

bool OrderBook::can_fill_completely(const Order* order) const {
    Quantity remaining = order->quantity;
    if (order->side == Side::Buy) {
        for (auto level_it = asks_.begin(); level_it != asks_.end() && remaining > 0; ++level_it) {
            if (order->type == OrderType::Limit && level_it->first > order->price)
                break;
            const Quantity available = level_it->second.total_qty_excluding(order->account_id);
            remaining = (available >= remaining) ? 0 : (remaining - available);
        }
    } else {
        for (auto level_it = bids_.begin(); level_it != bids_.end() && remaining > 0; ++level_it) {
            if (order->type == OrderType::Limit && level_it->first < order->price)
                break;
            const Quantity available = level_it->second.total_qty_excluding(order->account_id);
            remaining = (available >= remaining) ? 0 : (remaining - available);
        }
    }
    return remaining == 0;
}

bool OrderBook::would_cross(const Order* order) const {
    if (order->type == OrderType::Market)
        return true;
    if (order->side == Side::Buy) {
        return !asks_.empty() && asks_.begin()->first <= order->price;
    }
    return !bids_.empty() && bids_.begin()->first >= order->price;
}

bool OrderBook::crossed() const {
    if (bids_.empty() || asks_.empty())
        return false;
    return bids_.begin()->first >= asks_.begin()->first;
}

} // namespace exchange
