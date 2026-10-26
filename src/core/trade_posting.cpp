#include "trade_posting.hpp"

#include "exchange/core/order_book.hpp"
#include "exchange/text/wide_math.hpp"

namespace exchange::detail {

Price average_price(const std::vector<Trade>& trades, Quantity executed) {
    if (executed <= 0)
        return 0;

    NotionalSum notional;
    for (const Trade& trade : trades) {
        notional.add(trade.price, trade.quantity);
    }
    return notional.divide_by(executed);
}

void stamp_trades(std::vector<Trade>& trades, std::string_view symbol, std::int64_t timestamp_ns,
                  TradeID first_id) {
    TradeID next_id = first_id;
    for (Trade& trade : trades) {
        trade.id = next_id++;
        trade.symbol.assign(symbol.data(), symbol.size());
        trade.timestamp_ns = timestamp_ns;
    }
}

} // namespace exchange::detail
