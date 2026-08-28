/*
Nguyên tắc giao dịch:
- Khi có người mua thì sẽ được ghi vào sổ mua
(sổ luôn tự động sắp xếp theo thứ tự ưu tiên giá cao nhất đến giá thấp nhất)
- Khi có người bán thì sẽ được ghi vào sổ bán
(sổ luôn tự động sắp xếp theo thứ tự ưu tiên giá thấp nhất đến giá cao nhất)

- Khi có người đến bán:
+ Bước 1: Ghi lệnh vào sổ bán (đây là giá bán thấp nhất mà người bán chịu bán)
+ Bước 2: Check sổ mua và tìm lệnh đầu tiên của sổ đó (giá cao nhất)
+ Bước 3.1: Nếu giá bán (bước 1) <= giá mua (bước 2) => Khớp lệnh => Trade theo giá MUA và sửa sổ
cái (Order)
+ Bước 3.2: Nếu giá bán (bước 1) > giá mua (bước 2) => Không khớp lệnh => Chờ lần giao dịch sau

- Khi có người đến mua:
+ Bước 1: Ghi lệnh vào sổ mua (đây là giá mua cao nhất mà người mua chịu mua)
+ Bước 2: Check sổ bán và tìm lệnh đầu tiên của sổ đó (giá thấp nhất)
+ Bước 3.1: Nếu giá mua (bước 1) >= giá bán (bước 2) => Khớp lệnh => Trade theo giá BÁN và sửa sổ
cái (Order)
+ Bước 3.2: Nếu giá mua (bước 1) < giá bán (bước 2) => Không khớp lệnh => Chờ lần giao dịch sau
*/

/*
Đoạn code dưới đây là playground để test thuật toán khớp giao dịch
*/

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

/*
2 bên:
- `BUY`: Người mua
- `SELL`: Người bán
*/
enum class Side : char { BUY = 'B', SELL = 'S' };

/*
2 loại giao dịch:
- `LIMIT`: Tôi chỉ mua giá tối đa X, hoặc tôi chỉ bán giá tối thiểu Y
- `MARKET`: Tôi sẵn sàng mua với bất kỳ giá nào, hoặc tôi sẵn sàng bán với bất kỳ giá nào
*/
enum class OrderType : char { LIMIT = 'L', MARKET = 'M' };

/*
5 trạng thái của một lệnh yêu cầu giao dịch:
- `NEW`: Mới được chấp nhận nhưng chưa khớp với ai cả
- `PARTIALLY_FILLED`: Đã khớp một phần (ví dụ muốn mua 100 cổ phiếu nhưng mới khớp được 40 cổ phiếu)
- `FILLED`: Đã khớp hoàn toàn
- `CANCELLED`: Đã hủy (khi lệnh đang ở `NEW` hoặc `PARTIALLY_FILLED`)
- `REJECTED`: Bị từ chối (do tài khoản không đủ tiền, đặt sai biên độ giá sàn/trần,...)
*/
enum class OrderStatus { NEW, PARTIALLY_FILLED, FILLED, CANCELLED, REJECTED };

/*
Bảng `Order` - một lệnh yêu cầu giao dịch:
- `order_id`: Khóa chính
- `account_id`: ID tài khoản đặt lệnh
- `symbol_id`: ID mã chứng khoán
- `side`: Bên (mua/bán)
- `price`: Giá mua tối đa / Giá bán tối thiểu
- `initial_quantity`: Số lượng cổ phiếu muốn mua/bán ban đầu
- `remaining_quantity`: Số lượng cổ phiếu muốn mua/bán còn lại
- `timestamp_ns`: Dấu thời gian
*/
struct Order {
    uint64_t order_id;
    uint64_t account_id;
    uint32_t symbol_id;
    Side side;
    double price;
    uint32_t initial_quantity;
    uint32_t remaining_quantity;
    uint64_t timestamp_ns;
};

struct ComparePriceDesc {
    bool operator()(const Order& a, const Order& b) const {
        if (a.price == b.price) {
            return a.timestamp_ns > b.timestamp_ns;
        }
        return a.price < b.price;
    }
};

struct ComparePriceAsc {
    bool operator()(const Order& a, const Order& b) const {
        if (a.price == b.price) {
            return a.timestamp_ns > b.timestamp_ns;
        }
        return a.price > b.price;
    }
};

/*
Bảng `Trade` - một khớp lệnh đã hoàn tất:
- `trade_id`: Khóa chính
- `symbol_id`: ID mã chứng khoán
- `maker_order_id`: Khóa ngoại tới bảng Order*
- `taker_order_id`: Khóa ngoại tới bảng Order*
- `price`: Giá được khớp*
- `quantity`: Số lượng được khớp
- `timestamp_ns`: Dấu thời gian

** Maker là người tạo lập thị trường và Taker là người vét thanh khoản,
   ai (tức bên mua/bán) tới trước thì sẽ khớp giá của người đó
*/
struct Trade {
    uint64_t trade_id;
    uint32_t symbol_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    double price;
    uint32_t quantity;
    uint64_t timestamp_ns;
};

class MatchingEngine {
private:
    // Sổ ghi lệnh mua: Người mua nào trả giá cao hơn thì thể hiện thiện chí mua cao hơn và sẽ được
    // ưu tiên khớp trước.
    std::priority_queue<Order, std::vector<Order>, ComparePriceDesc> bids_;

    // Sổ ghi lệnh bán: Người bán nào chịu bán giá rẻ hơn thì sẽ hấp dẫn người mua hơn và được ưu
    // tiên khớp trước.
    std::priority_queue<Order, std::vector<Order>, ComparePriceAsc> asks_;

    std::unordered_map<uint64_t, Order> active_orders_;
    std::unordered_map<uint64_t, OrderStatus> order_statuses_;
    uint64_t next_trade_id_ = 1;

public:
    std::vector<Trade> AddOrder(Order new_order) {
        std::vector<Trade> executed_trades;
        order_statuses_[new_order.order_id] = OrderStatus::NEW;

        if (new_order.side == Side::BUY) {
            while (new_order.remaining_quantity > 0 && !asks_.empty()) {
                Order best_ask = asks_.top();

                if (order_statuses_[best_ask.order_id] == OrderStatus::CANCELLED) {
                    asks_.pop();
                    continue;
                }
                bool price_matches =
                    (new_order.price == 0 /* quy ước market price tạm thời */ ||
                     new_order.price >=
                         best_ask.price);

                bool is_market =
                    (new_order.price <=
                     0);  // ví dụ thế, hoặc bạn thêm trường OrderType type vào struct Order

                // Tạm thời viết theo logic: So sánh giá trực tiếp
                if (is_market || new_order.price >= best_ask.price) {
                    asks_.pop();  // Lấy ra khỏi hàng đợi cũ vì ta sẽ cập nhật lại số lượng

                    uint32_t trade_qty =
                        std::min(new_order.remaining_quantity, best_ask.remaining_quantity);
                    double execution_price = best_ask.price;  // Maker (best_ask) quyết định giá

                    // Tạo Trade mới
                    executed_trades.push_back(Trade{.trade_id = next_trade_id_++,
                                                    .symbol_id = new_order.symbol_id,
                                                    .maker_order_id = best_ask.order_id,
                                                    .taker_order_id = new_order.order_id,
                                                    .price = execution_price,
                                                    .quantity = trade_qty,
                                                    .timestamp_ns = new_order.timestamp_ns});

                    // Cập nhật số lượng còn lại
                    new_order.remaining_quantity -= trade_qty;
                    best_ask.remaining_quantity -= trade_qty;

                    // Cập nhật trạng thái Maker
                    if (best_ask.remaining_quantity == 0) {
                        order_statuses_[best_ask.order_id] = OrderStatus::FILLED;
                    } else {
                        order_statuses_[best_ask.order_id] = OrderStatus::PARTIALLY_FILLED;
                        asks_.push(best_ask);  // Đẩy phần dư còn lại của Maker trở lại queue
                    }
                } else {
                    break;  // Không khớp được nữa vì giá không thỏa mãn
                }
            }

            // Xử lý phần còn dư của Taker (new_order)
            if (new_order.remaining_quantity == 0) {
                order_statuses_[new_order.order_id] = OrderStatus::FILLED;
            } else if (new_order.remaining_quantity < new_order.initial_quantity) {
                order_statuses_[new_order.order_id] = OrderStatus::PARTIALLY_FILLED;
                bids_.push(new_order);  // Đẩy vào sổ Mua để chờ khớp tiếp
                active_orders_[new_order.order_id] = new_order;
            } else {
                // Không khớp được chút nào -> Treo nguyên lệnh lên sổ Mua
                bids_.push(new_order);
                active_orders_[new_order.order_id] = new_order;
            }
        }
        // Xử lý lệnh BÁN (SELL) - Tương tự đối xứng với BUY
        else {
            while (new_order.remaining_quantity > 0 && !bids_.empty()) {
                Order best_bid = bids_.top();

                if (order_statuses_[best_bid.order_id] == OrderStatus::CANCELLED) {
                    bids_.pop();
                    continue;
                }

                if (new_order.price <= 0 || new_order.price <= best_bid.price) {
                    bids_.pop();

                    uint32_t trade_qty =
                        std::min(new_order.remaining_quantity, best_bid.remaining_quantity);
                    double execution_price = best_bid.price;  // Maker (best_bid) quyết định giá

                    executed_trades.push_back(Trade{.trade_id = next_trade_id_++,
                                                    .symbol_id = new_order.symbol_id,
                                                    .maker_order_id = best_bid.order_id,
                                                    .taker_order_id = new_order.order_id,
                                                    .price = execution_price,
                                                    .quantity = trade_qty,
                                                    .timestamp_ns = new_order.timestamp_ns});

                    new_order.remaining_quantity -= trade_qty;
                    best_bid.remaining_quantity -= trade_qty;

                    if (best_bid.remaining_quantity == 0) {
                        order_statuses_[best_bid.order_id] = OrderStatus::FILLED;
                    } else {
                        order_statuses_[best_bid.order_id] = OrderStatus::PARTIALLY_FILLED;
                        bids_.push(best_bid);
                    }
                } else {
                    break;
                }
            }

            if (new_order.remaining_quantity == 0) {
                order_statuses_[new_order.order_id] = OrderStatus::FILLED;
            } else if (new_order.remaining_quantity < new_order.initial_quantity) {
                order_statuses_[new_order.order_id] = OrderStatus::PARTIALLY_FILLED;
                asks_.push(new_order);
                active_orders_[new_order.order_id] = new_order;
            } else {
                asks_.push(new_order);
                active_orders_[new_order.order_id] = new_order;
            }
        }

        return executed_trades;
    }

    bool CancelOrder(uint64_t order_id) {
        // Kiểm tra xem lệnh có tồn tại và đang ở trạng thái có thể hủy không
        if (order_statuses_.find(order_id) == order_statuses_.end()) {
            return false;
        }

        OrderStatus current_status = order_statuses_[order_id];
        if (current_status == OrderStatus::NEW || current_status == OrderStatus::PARTIALLY_FILLED) {
            order_statuses_[order_id] = OrderStatus::CANCELLED;
            active_orders_.erase(order_id);
            return true;
        }

        return false;  // Đã khớp hết (FILLED) hoặc đã hủy/từ chối từ trước thì không hủy được nữa
    }
};

void PrintTrades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "  [TRADE] ID: " << t.trade_id << " | Maker ID: " << t.maker_order_id
                  << " | Taker ID: " << t.taker_order_id << " | Price: " << t.price
                  << " | Qty: " << t.quantity << "\n";
    }
}

int main() {
    MatchingEngine engine;

    std::cout << "=== TEST 1: Treo lenh ban (Maker) len so Asks ===\n";
    // Ông A mang 50kg khoai ra bán giá 20.0 (Order ID: 1)
    std::vector<Trade> t1 = engine.AddOrder(Order{.order_id = 1,
                                                  .account_id = 101,
                                                  .symbol_id = 1,
                                                  .side = Side::SELL,
                                                  .price = 20.0,
                                                  .initial_quantity = 50,
                                                  .remaining_quantity = 50,
                                                  .timestamp_ns = 1000});
    std::cout << "Trades executed: " << t1.size() << "\n\n";

    std::cout << "=== TEST 2: Nguoi mua (Taker) nhay vao khop mot phan (PARTIALLY_FILLED) ===\n";
    // Ông B muốn mua 30kg khoai, trả giá 22.0 (cao hơn giá ông A bán, nên khớp luôn) (Order ID: 2)
    std::vector<Trade> t2 = engine.AddOrder(Order{.order_id = 2,
                                                  .account_id = 102,
                                                  .symbol_id = 1,
                                                  .side = Side::BUY,
                                                  .price = 22.0,
                                                  .initial_quantity = 30,
                                                  .remaining_quantity = 30,
                                                  .timestamp_ns = 2000});
    std::cout << "Trades executed: " << t2.size() << "\n";
    PrintTrades(t2);
    // Lúc này ông A còn dư 20kg trên sổ Asks, ông B đã mua đủ 30kg (FILLED).
    std::cout << "\n";

    std::cout << "=== TEST 3: Nguoi mua khop not phan du va vet sach so Asks (FILLED) ===\n";
    // Ông C muốn mua 40kg khoai, giá 20.0 (Order ID: 3)
    // Sẽ khớp 20kg còn lại của ông A, và 20kg còn thừa sẽ được treo lên sổ Bids.
    std::vector<Trade> t3 = engine.AddOrder(Order{.order_id = 3,
                                                  .account_id = 103,
                                                  .symbol_id = 1,
                                                  .side = Side::BUY,
                                                  .price = 20.0,
                                                  .initial_quantity = 40,
                                                  .remaining_quantity = 40,
                                                  .timestamp_ns = 3000});
    std::cout << "Trades executed: " << t3.size() << "\n";
    PrintTrades(t3);
    std::cout << "\n";

    std::cout << "=== TEST 4: Huy lenh (CANCELLED) ===\n";
    // Do ông C lúc nãy đặt mua 40kg nhưng mới khớp được 20kg, còn dư 20kg đang treo trên sổ Bids
    // (Order ID: 3). Bây giờ ông C đổi ý, gọi điện hủy lệnh này.
    bool cancel_result = engine.CancelOrder(3);
    std::cout << "Cancel Order ID 3 result: " << (cancel_result ? "SUCCESS" : "FAILED") << "\n";

    // Thử hủy lại lần nữa xem có từ chối không
    bool cancel_again = engine.CancelOrder(3);
    std::cout << "Cancel Order ID 3 again result: " << (cancel_again ? "SUCCESS" : "FAILED")
              << "\n";

    return 0;
}