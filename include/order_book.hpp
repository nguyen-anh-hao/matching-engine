#pragma once
#include <functional>
#include <list>
#include <map>
#include <vector>

#include "types.hpp"

class OrderBook {
public:
    explicit OrderBook(uint32_t symbol_id);

    std::vector<Trade> add_order(Order order, uint64_t& trade_id_counter);

    bool cancel_order(uint64_t order_id, Side side, double price);

    uint32_t get_symbol_id() const {
        return symbol_id_;
    }

private:
    uint32_t symbol_id_;

    std::map<double, std::list<Order>, std::greater<double>> bids_;

    std::map<double, std::list<Order>, std::less<double>> asks_;
};