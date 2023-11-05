#pragma once

#include <functional>
#include <map>

#include "boost/intrusive/intrusive_fwd.hpp"
#include "boost/intrusive/list.hpp"
#include "boost/intrusive/rbtree.hpp"
#include "orderqueue.hpp"
#include "pool.hpp"

namespace orderbook {

class OrderBook;

class Compare {
   public:
    bool LessThanOrEqual(Decimal price);
    bool GreaterThanOrEqual(Decimal price);
};

using CmpGreater = boost::intrusive::compare<std::greater<>>;
using CmpLess = boost::intrusive::compare<std::less<>>;

template <class CompareType>
class PriceLevel {
    pool::AdaptiveObjectPool<OrderQueue> queue_pool_;

    using PriceTree = boost::intrusive::rbtree<OrderQueue, CompareType>;
    PriceTree price_tree_;

    PriceType price_type_;
    Decimal volume_;
    uint64_t num_orders_ = 0;
    uint64_t depth_ = 0;

   public:
    PriceLevel(PriceType price_type, size_t price_level_pool_size) : price_type_(price_type), queue_pool_(price_level_pool_size){};
    uint64_t len();
    uint64_t depth();
    Decimal volume();
    OrderQueue* getQueue();
    void append(Order* order);
    void remove(const std::shared_ptr<Order>& order);
    Decimal processMarketOrder(OrderBook& ob, OrderID takerOrderID, Decimal qty, Flag flag);
    Decimal processLimitOrder(OrderBook& ob, OrderID& takerOrderID, Decimal& price, Decimal qty, Flag& flag);

    PriceTree& price_tree() { return price_tree_; }

    OrderQueue* LargestLessThan(const Decimal& price) {
        auto it = price_tree_.upper_bound(price, PriceCompare());
        if (it != price_tree_.begin()) {
            --it;
            return &(*it);
        }
        return nullptr;
    }

    OrderQueue* SmallestGreaterThan(const Decimal& price) {
        auto it = price_tree_.lower_bound(price, PriceCompare());
        if (it != price_tree_.end()) {
            return &(*it);
        }
        return nullptr;
    }

    OrderQueue* GetNextQueue(const Decimal& price) {
        switch (price_type_) {
            case PriceType::Bid:
                return LargestLessThan(price);
            case PriceType::Ask:
                return SmallestGreaterThan(price);
            default:
                throw std::runtime_error("invalid call to GetQueue");
        }
    }
};

}  // namespace orderbook

