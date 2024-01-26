#pragma once

#include <cstdint>
#include <functional>

#include "boost/intrusive/list.hpp"
#include "boost/intrusive/set_hook.hpp"
#include "order.hpp"

namespace orderbook {

using TradeNotification =
    std::function<void(OrderID mOrderID, OrderID tOrderID, OrderStatus mOrderStatus, OrderStatus tOrderStatus, Decimal qty, Decimal price)>;
using PostOrderFill = std::function<void(OrderID canceledOrderID)>;

using OrderList = boost::intrusive::list<Order>;

class OrderQueue : public boost::intrusive::set_base_hook<boost::intrusive::optimize_size<false>> {
    OrderList orders_;

    Decimal price_;
    Decimal total_qty_;

   public:
    OrderQueue(const Decimal &price) : price_(price){};
    [[nodiscard]] Decimal price() const;
    uint64_t len();
    [[nodiscard]] Decimal totalQty() const;
    [[nodiscard]] Order *head();
    [[nodiscard]] Order *tail();
    void append(Order *o);
    void remove(Order *o);
    Decimal process(const TradeNotification &tn, const PostOrderFill &postFill, OrderID takerOrderID, Decimal qty);

    friend bool operator<(const OrderQueue &a, const OrderQueue &b) { return a.price_ < b.price_; }
    friend bool operator>(const OrderQueue &a, const OrderQueue &b) { return a.price_ > b.price_; }
    friend bool operator==(const OrderQueue &a, const OrderQueue &b) { return a.price_ == b.price_; }

    friend class PriceCompare;
};

struct PriceCompare {
    bool operator()(const OrderQueue &q1, const OrderQueue &q2) const { return q1.price_ < q2.price_; }

    bool operator()(const OrderQueue &q, const Decimal &price) const { return q.price_ < price; }

    bool operator()(const Decimal &price, const OrderQueue &q) const { return price < q.price_; }
};

}  // namespace orderbook
