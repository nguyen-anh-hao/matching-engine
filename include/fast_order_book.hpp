#pragma once
#include <vector>

#include "order_pool.hpp"
#include "types.hpp"

struct PriceLevel {
    int32_t head_idx{-1};
    int32_t tail_idx{-1};
    Quantity total_volume{0};
};

class FastOrderBook {
public:
    FastOrderBook();

    // Khớp lệnh trực tiếp không qua mutex
    void process_order(const IngestOrderCommand& cmd, std::vector<Trade>& out_trades);
    bool cancel_order(OrderId order_id, Price price, Side side);

private:
    void match_buy(const IngestOrderCommand& cmd, std::vector<Trade>& out_trades);
    void match_sell(const IngestOrderCommand& cmd, std::vector<Trade>& out_trades);

    void add_to_book(int32_t order_idx, Price price, Side side);
    void remove_from_book(int32_t order_idx, Price price, Side side);

    OrderPool pool_;
    PriceLevel bids_[MAX_PRICE + 1];  // Direct indexing theo mức giá
    PriceLevel asks_[MAX_PRICE + 1];

    Price best_bid_{0};
    Price best_ask_{MAX_PRICE};
};