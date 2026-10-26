#pragma once

#include "exchange/core/order.hpp"

#include <cstddef>
#include <deque>

namespace exchange {

// Pre-allocated pool of orders. Storage grows monotonically (deque push_back
// never invalidates pointers to existing elements) and freed orders are
// recycled through an intrusive free-list, so the matching path performs no
// heap allocation for order storage.
class OrderPool {
public:
    OrderPool() = default;

    // The pool owns the only pointers to its storage; copying would duplicate
    // the free list and hand out the same order twice.
    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;
    OrderPool(OrderPool&&) = delete;
    OrderPool& operator=(OrderPool&&) = delete;

    Order* acquire() {
        if (free_list_ != nullptr) {
            Order* order = free_list_;
            free_list_ = order->next;
            order->reset();
            return order;
        }
        storage_.emplace_back();
        return &storage_.back();
    }

    void release(Order* order) {
        order->reset();
        order->next = free_list_;
        free_list_ = order;
    }

    size_t capacity() const { return storage_.size(); }

private:
    std::deque<Order> storage_;
    Order* free_list_ = nullptr;
};

} // namespace exchange
