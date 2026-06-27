#pragma once

#include <cstddef>
#include <cstdint>

#include "array_levels.hpp"
#include "level_store.hpp"
#include "orderqueue.hpp"
#include "types.hpp"

namespace orderbook {

class Compare {
   public:
    bool LessThanOrEqual(Decimal price);
    bool GreaterThanOrEqual(Decimal price);
};

// PriceLevel owns the per-side level accounting (volume_ / num_orders_) and the
// matching loops (processMarketOrder / processLimitOrder). The price-level
// CONTAINER itself is a compile-time policy `Store` (a LevelStore backend over
// PriceType P): ArrayLevels<P> (default, tick array + bitmap) or RbTreeLevels<P>
// (boost::intrusive::rbtree). All store methods are header-defined and inline,
// so there is no virtual dispatch on the hot path and the matching logic is
// shared by both backends rather than duplicated.
template <PriceType P, class Store = ArrayLevels<P>>
class PriceLevel {
    Store store_;

    PriceType price_type_ = P;
    Decimal volume_;
    uint64_t num_orders_ = 0;

   public:
    explicit PriceLevel(const LevelStoreConfig& cfg) : store_(cfg) {}

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
};

}  // namespace orderbook
