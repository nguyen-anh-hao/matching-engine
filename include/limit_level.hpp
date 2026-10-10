#pragma once

#include "order.hpp"
#include "types.hpp"

namespace exchange {

// FIFO intrusive doubly-linked list of orders resting at a single price level.
// Head of the list is the order with the earliest time priority.
class LimitLevel {
public:
    void append(Order* o) {
        o->prev = tail_;
        o->next = nullptr;
        if (tail_) {
            tail_->next = o;
        } else {
            head_ = o;
        }
        tail_ = o;
        ++count_;
        total_qty_ += o->leaves;
    }

    // Remove an order from the level. Subtracts the order's *remaining* leaves.
    void remove(Order* o) {
        if (o->prev) o->prev->next = o->next;
        else head_ = o->next;
        if (o->next) o->next->prev = o->prev;
        else tail_ = o->prev;
        o->prev = o->next = nullptr;
        --count_;
        total_qty_ -= o->leaves;
    }

    // Reduce the open quantity of an order that is being filled.
    void reduce(Order* o, Quantity by) {
        total_qty_ -= by;
        o->leaves -= by;
        o->executed += by;
    }

    Order* head() const { return head_; }
    bool empty() const { return head_ == nullptr; }
    uint32_t count() const { return count_; }
    Quantity total_qty() const { return total_qty_; }

    // Total open quantity at this level excluding orders owned by `id`, used by
    // the FOK pre-check to stay consistent with self-trade prevention in match().
    Quantity total_qty_excluding(AccountID id) const {
        Quantity sum = 0;
        for (Order* o = head_; o != nullptr; o = o->next) {
            if (o->account_id != id) sum += o->leaves;
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
