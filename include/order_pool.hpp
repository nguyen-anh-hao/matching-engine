#pragma once
#include <vector>

#include "types.hpp"

class OrderPool {
public:
    explicit OrderPool(size_t capacity = MAX_ORDERS_PER_BOOK);

    int32_t allocate();
    void deallocate(int32_t index);
    PoolOrder& get(int32_t index) {
        return pool_[index];
    }
    const PoolOrder& get(int32_t index) const {
        return pool_[index];
    }

private:
    std::vector<PoolOrder> pool_;
    std::vector<int32_t> free_list_;
    int32_t free_top_{0};
};