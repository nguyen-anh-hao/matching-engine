#include "order_book.hpp"

#include <algorithm>
#include <utility>

namespace exchange {

void OrderBook::add(Order* o) {
    index_[o->id] = o;
    if (o->side == Side::Buy) {
        bids_[o->price].append(o);
    } else {
        asks_[o->price].append(o);
    }
}

bool OrderBook::remove(Order* o) {
    auto it = index_.find(o->id);
    if (it == index_.end()) return false;

    LimitLevel* level = nullptr;
    if (o->side == Side::Buy) {
        auto lit = bids_.find(o->price);
        if (lit != bids_.end()) level = &lit->second;
    } else {
        auto lit = asks_.find(o->price);
        if (lit != asks_.end()) level = &lit->second;
    }
    if (level) level->remove(o);
    if (level && level->empty()) {
        if (o->side == Side::Buy) bids_.erase(o->price);
        else asks_.erase(o->price);
    }
    index_.erase(it);
    return true;
}

Order* OrderBook::find(OrderID id) const {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : it->second;
}

void OrderBook::match(Order* incoming, std::vector<Trade>& trades) {
    const bool is_buy = (incoming->side == Side::Buy);

    // Match against the opposite side using price-time priority, skipping the
    // incoming account's own resting orders (self-trade prevention).
    auto match_against = [&](auto& opposite) {
        auto it = opposite.begin();
        while (it != opposite.end() && incoming->leaves > 0) {
            const Price level_price = it->first;
            if (incoming->type == OrderType::Limit) {
                if (is_buy && level_price > incoming->price) break;
                if (!is_buy && level_price < incoming->price) break;
            }
            LimitLevel& level = it->second;
            Order* resting = level.head();
            while (resting != nullptr && incoming->leaves > 0) {
                Order* next = resting->next; // save before any removal
                if (resting->account_id != incoming->account_id) {
                    Quantity match_qty = std::min(incoming->leaves, resting->leaves);
                    level.reduce(resting, match_qty);
                    incoming->leaves -= match_qty;
                    incoming->executed += match_qty;

                    trades.push_back(Trade{0,
                                           incoming->get_symbol(),
                                           resting->price,
                                           match_qty,
                                           resting->id,
                                           incoming->id,
                                           resting->account_id,
                                           incoming->account_id,
                                           now_ns()});

                    if (resting->leaves == 0) {
                        level.remove(resting);
                        resting->status = OrderStatus::Filled;
                        index_.erase(resting->id);
                    } else {
                        resting->status = OrderStatus::PartiallyFilled;
                    }
                }
                resting = next;
            }
            if (level.empty()) it = opposite.erase(it);
            else ++it;
        }
    };

    if (is_buy) {
        match_against(asks_);
    } else {
        match_against(bids_);
    }
}

bool OrderBook::can_fill_completely(const Order* o) const {
    Quantity remaining = o->quantity;
    if (o->side == Side::Buy) {
        for (auto it = asks_.begin(); it != asks_.end() && remaining > 0; ++it) {
            if (o->type == OrderType::Limit && it->first > o->price) break;
            Quantity avail = it->second.total_qty_excluding(o->account_id);
            remaining = (avail >= remaining) ? 0 : (remaining - avail);
        }
    } else {
        for (auto it = bids_.begin(); it != bids_.end() && remaining > 0; ++it) {
            if (o->type == OrderType::Limit && it->first < o->price) break;
            Quantity avail = it->second.total_qty_excluding(o->account_id);
            remaining = (avail >= remaining) ? 0 : (remaining - avail);
        }
    }
    return remaining == 0;
}

} // namespace exchange
