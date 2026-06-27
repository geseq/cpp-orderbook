#include "pricelevel.hpp"

#include <cstdint>

#include "array_levels.hpp"
#include "rbtree_levels.hpp"

namespace orderbook {

// All logic below is COMMON to every LevelStore backend: it routes container
// operations through store_ and keeps the volume_ / num_orders_ accounting and
// the matching loops in one place. The store (ArrayLevels / RbTreeLevels) only
// owns the price-level container and the OrderQueue pool.

template <PriceType P, class Store>
uint64_t PriceLevel<P, Store>::len() {
    return num_orders_;
}

template <PriceType P, class Store>
uint64_t PriceLevel<P, Store>::depth() {
    return store_.depth();
}

template <PriceType P, class Store>
Decimal PriceLevel<P, Store>::volume() {
    return volume_;
}

template <PriceType P, class Store>
void PriceLevel<P, Store>::append(Order* order) {
    OrderQueue* q = store_.findOrCreate(order->price);

    ++num_orders_;
    volume_ += order->qty;
    order->queue = q;
    q->append(order);
}

template <PriceType P, class Store>
void PriceLevel<P, Store>::remove(Order* order) {
    // Use the order's queue back-pointer (set on append); backend-agnostic.
    OrderQueue* q = order->queue;
    if (q != nullptr) {
        q->remove(order);
        if (q->len() == 0) {
            store_.erase(q);
        }
    }

    --num_orders_;
    volume_ -= order->qty;
}

template <PriceType P, class Store>
OrderQueue* PriceLevel<P, Store>::getQueue() {
    return store_.best();
}

template <PriceType P, class Store>
template <PriceType Q>
Decimal PriceLevel<P, Store>::processMarketOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal qty, Flag flag) {
    static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");

    if ((flag & (AoN | FoK)) != 0 && qty > volume_) {
        // AoN/FoK must match the full requested quantity; return zero processed when aggregate volume is insufficient.
        return Decimal{};
    }

    auto qtyLeft = qty;
    Decimal qtyProcessed = uint64_t(0);
    for (auto q = getQueue(); !qtyLeft.is_zero() && q != nullptr; q = getQueue()) {
        auto pq = q->process(tn, pf, takerOrderID, qtyLeft);
        qtyLeft -= pq;
        qtyProcessed += pq;
        volume_ -= pq;
    }

    return qtyProcessed;
};

template <PriceType P, class Store>
template <PriceType Q>
Decimal PriceLevel<P, Store>::processLimitOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal price, Decimal qty, Flag flag) {
    static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");
    Decimal qtyProcessed = {};
    auto orderQueue = getQueue();

    if (orderQueue == nullptr) {
        return qtyProcessed;
    }

    if constexpr (P == PriceType::Bid) {
        if (price > orderQueue->price()) {
            return qtyProcessed;
        }
    } else {
        if (price < orderQueue->price()) {
            return qtyProcessed;
        }
    }

    // AoN/FoK pre-check: only continue when aggregate fillable volume exists at eligible price levels.
    // Matched quantity is accounted for incrementally via volume_ -= result in the execution loop below.
    // The best-to-worst walk uses getQueue() + getNextQueue(), so it is backend-agnostic.
    if (flag & (AoN | FoK)) {
        if (qty > volume()) {
            return Decimal{};
        }

        bool canFill = false;
        auto aQty = qty;
        for (auto* q = getQueue(); q != nullptr; q = getNextQueue(q->price())) {
            if constexpr (P == PriceType::Bid) {
                if (q->price() < price) {
                    break;
                }
            } else {
                if (q->price() > price) {
                    break;
                }
            }
            if (aQty <= q->totalQty()) {
                canFill = true;
                break;
            }
            aQty -= q->totalQty();
        }

        if (!canFill) {
            return Decimal{};
        }
    }

    Decimal qtyLeft = qty;

    for (orderQueue = getQueue(); !qtyLeft.is_zero() && orderQueue != nullptr; orderQueue = getQueue()) {
        // Stop as soon as the best remaining queue price no longer satisfies the limit.
        if constexpr (P == PriceType::Bid) {
            if (orderQueue->price() < price) break;  // bid price fell below sell limit
        } else {
            if (orderQueue->price() > price) break;  // ask price rose above buy limit
        }
        Decimal result = orderQueue->process(tn, pf, takerOrderID, qtyLeft);
        qtyLeft -= result;
        qtyProcessed += result;
        volume_ -= result;
    }

    return qtyProcessed;
};

template <PriceType P, class Store>
OrderQueue* PriceLevel<P, Store>::largestLessThan(const Decimal& price) {
    return store_.below(price);
}

template <PriceType P, class Store>
OrderQueue* PriceLevel<P, Store>::smallestGreaterThan(const Decimal& price) {
    return store_.above(price);
}

template <PriceType P, class Store>
template <PriceType Q>
OrderQueue* PriceLevel<P, Store>::getNextQueue(const Decimal& price) {
    if constexpr (Q == PriceType::Bid) {
        return largestLessThan(price);
    } else if constexpr (Q == PriceType::Ask) {
        return smallestGreaterThan(price);
    } else {
        static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");
    }
}

// --- Explicit instantiations for BOTH backends ------------------------------
// PriceLevel methods stay in this TU (callback bodies + matching loops here) and
// are explicitly instantiated for each (PriceType, Store) pair. Under LTO the
// store calls within them inline; without LTO they inline within this TU.

#define INSTANTIATE_PRICELEVEL(P, STORE)                                                                                                              \
    template class PriceLevel<P, STORE>;                                                                                                              \
    template Decimal PriceLevel<P, STORE>::processMarketOrder<P>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,          \
                                                                 Decimal qty, Flag flag);                                                             \
    template Decimal PriceLevel<P, STORE>::processLimitOrder<P>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,           \
                                                                Decimal price, Decimal qty, Flag flag);                                               \
    template OrderQueue* PriceLevel<P, STORE>::getNextQueue<P>(const Decimal& price);

INSTANTIATE_PRICELEVEL(PriceType::Bid, ArrayLevels<PriceType::Bid>)
INSTANTIATE_PRICELEVEL(PriceType::Ask, ArrayLevels<PriceType::Ask>)
INSTANTIATE_PRICELEVEL(PriceType::Bid, RbTreeLevels<PriceType::Bid>)
INSTANTIATE_PRICELEVEL(PriceType::Ask, RbTreeLevels<PriceType::Ask>)

#undef INSTANTIATE_PRICELEVEL

}  // namespace orderbook
