#pragma once

#include "exchange/core/order.hpp"
#include "exchange/core/types.hpp"

#include <cassert>
#include <cstdint>

namespace exchange {

class LimitLevel {
public:
    LimitLevel() = default;

    LimitLevel(const LimitLevel&) = delete;
    LimitLevel& operator=(const LimitLevel&) = delete;
    LimitLevel(LimitLevel&&) = delete;
    LimitLevel& operator=(LimitLevel&&) = delete;

    void append(Order* order) {
        assert(order != nullptr);
        order->prev = tail_;
        order->next = nullptr;
        if (tail_ != nullptr) {
            tail_->next = order;
        } else {
            head_ = order;
        }
        tail_ = order;
        ++count_;
        total_qty_ += order->leaves;
    }

    void remove(Order* order) {
        assert(count_ > 0);
        if (order->prev != nullptr) {
            order->prev->next = order->next;
        } else {
            head_ = order->next;
        }
        if (order->next != nullptr) {
            order->next->prev = order->prev;
        } else {
            tail_ = order->prev;
        }
        order->prev = nullptr;
        order->next = nullptr;
        --count_;
        total_qty_ -= order->leaves;
    }

    void reduce(Order* order, Quantity by) {
        assert(by > 0 && by <= order->leaves);
        total_qty_ -= by;
        order->leaves -= by;
        order->executed += by;
    }

    Order* head() const { return head_; }
    bool empty() const { return head_ == nullptr; }
    uint32_t count() const { return count_; }
    Quantity total_qty() const { return total_qty_; }

    Quantity total_qty_excluding(AccountID id) const {
        Quantity sum = 0;
        for (const Order* order = head_; order != nullptr; order = order->next) {
            if (order->account_id != id)
                sum += order->leaves;
        }
        return sum;
    }

private:
    Order* head_ = nullptr;
    Order* tail_ = nullptr;
    uint32_t count_ = 0;
    Quantity total_qty_ = 0;
};

} // namespace exchange
