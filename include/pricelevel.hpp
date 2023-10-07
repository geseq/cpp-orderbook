#pragma once

#include <functional>
#include <map>

#include "boost/intrusive/intrusive_fwd.hpp"
#include "boost/intrusive/list.hpp"
#include "boost/intrusive/rbtree.hpp"
#include "orderqueue.hpp"

namespace orderbook {

class OrderBook;

class Compare {
   public:
    bool LessThanOrEqual(Decimal price);
    bool GreaterThanOrEqual(Decimal price);
};

using CmpGreater = boost::intrusive::compare<std::greater<>>;
using CmpLess = boost::intrusive::compare<std::less<>>;

using OrderList = boost::intrusive::list<Order>;

template <class CompareType>
class PriceLevel {
    boost::intrusive::rbtree<OrderQueue, CompareType> price_tree_;

    PriceType price_type_;
    Decimal volume_;
    uint64_t num_orders_ = 0;
    uint64_t depth_ = 0;
    OrderList list_;

   public:
    PriceLevel(PriceType price_type) : price_type_(price_type){};
    uint64_t len();
    uint64_t depth();
    Decimal volume();
    OrderQueue* getQueue();
    void append(const std::shared_ptr<Order>& order);
    void remove(const std::shared_ptr<Order>& order);
    Decimal processMarketOrder(OrderBook* ob, OrderID takerOrderID, Decimal qty, Flag flag);
    Decimal processLimitOrder(OrderBook* ob, bool(Decimal), OrderID takerOrderID, Decimal qty, Flag flag);
};

}  // namespace orderbook

