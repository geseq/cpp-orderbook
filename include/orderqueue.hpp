#pragma once

#include <cstdint>

#include "boost/intrusive/set_hook.hpp"
#include "order.hpp"

namespace orderbook {

class OrderBook;

class OrderQueue : public boost::intrusive::set_base_hook<boost::intrusive::optimize_size<false>> {
    std::shared_ptr<Order> head_ = nullptr;
    std::shared_ptr<Order> tail_ = nullptr;

    Decimal total_qty_;
    uint64_t size_ = 0;

   public:
    OrderQueue(Decimal price) : price_(price){};
    Decimal price();
    uint64_t len();
    uint64_t totalQty();
    std::shared_ptr<Order> head();
    void append(std::shared_ptr<Order> o);
    std::shared_ptr<Order> remove(const std::shared_ptr<Order> &o);
    Decimal process(OrderBook *ob, OrderID takerOrderID, Decimal qty);

    Decimal price_;

    friend bool operator<(const OrderQueue &a, const OrderQueue &b) { return a.price_ < b.price_; }
    friend bool operator>(const OrderQueue &a, const OrderQueue &b) { return a.price_ > b.price_; }
    friend bool operator==(const OrderQueue &a, const OrderQueue &b) { return a.price_ == b.price_; }
};

struct PriceCompare {
    bool operator()(const OrderQueue &q1, const OrderQueue &q2) const { return q1.price_ < q2.price_; }

    bool operator()(const OrderQueue &q, const Decimal &price) const { return q.price_ < price; }

    bool operator()(const Decimal &price, const OrderQueue &q) const { return price < q.price_; }
};

}  // namespace orderbook
