#include <cstdint>

#include "order.hpp"

namespace orderbook {

class OrderBook;

class OrderQueue {
    std::shared_ptr<Order> head_ = nullptr;
    std::shared_ptr<Order> tail_ = nullptr;

    Decimal price_ = 0;
    Decimal total_qty_ = 0;
    uint64_t size_ = 0;

   public:
    OrderQueue(Decimal price) : price_(price){};
    Decimal price();
    uint64_t len();
    uint64_t totalQty();
    std::shared_ptr<Order> head();
    void append(std::shared_ptr<Order> o);
    std::shared_ptr<Order> remove(const std::shared_ptr<Order>& o);
    Decimal process(OrderBook* ob, OrderId takerOrderId, Decimal qty);
};

}  // namespace orderbook
