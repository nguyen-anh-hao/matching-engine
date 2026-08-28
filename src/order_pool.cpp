#include "order_pool.hpp"

OrderPool::OrderPool(size_t capacity) : pool_(capacity), free_list_(capacity) {
    for (size_t i = 0; i < capacity; ++i) {
        free_list_[i] = static_cast<int32_t>(capacity - 1 - i);
    }
    free_top_ = static_cast<int32_t>(capacity);
}

int32_t OrderPool::allocate() {
    if (free_top_ <= 0)
        return -1;  // Out of memory
    return free_list_[--free_top_];
}

void OrderPool::deallocate(int32_t index) {
    free_list_[free_top_++] = index;
}