#pragma once

#include <functional>
#include <map>

#include "boost/intrusive/intrusive_fwd.hpp"
#include "boost/intrusive/list.hpp"
#include "boost/intrusive/rbtree.hpp"
#include "orderqueue.hpp"
#include "pool.hpp"
#include "types.hpp"

namespace orderbook {

class Compare {
   public:
    bool LessThanOrEqual(Decimal price);
    bool GreaterThanOrEqual(Decimal price);
};

using CmpGreater = boost::intrusive::compare<std::greater<>>;
using CmpLess = boost::intrusive::compare<std::less<>>;

template <PriceType P>
class PriceLevel {
    pool::AdaptiveObjectPool<OrderQueue> queue_pool_;

    using CompareType = std::conditional_t<(P == PriceType::Bid || P == PriceType::TriggerOver), CmpGreater, CmpLess>;
    using PriceTree = boost::intrusive::rbtree<OrderQueue, CompareType>;
    PriceTree price_tree_;

    PriceType price_type_ = P;
    Decimal volume_;
    uint64_t num_orders_ = 0;
    uint64_t depth_ = 0;

   public:
    PriceLevel(size_t price_level_pool_size) : queue_pool_(price_level_pool_size){};
    uint64_t len();
    uint64_t depth();
    Decimal volume();
    [[nodiscard]] OrderQueue* getQueue();
    [[nodiscard]] OrderQueue* largestLessThan(const Decimal& price);
    [[nodiscard]] OrderQueue* smallestGreaterThan(const Decimal& price);

    void append(Order* order);
    void remove(Order* order);

    template <PriceType Q = P>
    [[nodiscard]] OrderQueue* getNextQueue(const Decimal& price);

    template <PriceType Q = P>
    Decimal processMarketOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal qty, Flag flag);

    template <PriceType Q = P>
    Decimal processLimitOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal price, Decimal qty, Flag flag);

    const PriceTree& price_tree() const { return price_tree_; };
};

}  // namespace orderbook

