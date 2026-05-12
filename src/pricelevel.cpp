#include "pricelevel.hpp"

#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>

namespace orderbook {

template <PriceType P>
uint64_t PriceLevel<P>::len() {
    return num_orders_;
}

template <PriceType P>
uint64_t PriceLevel<P>::depth() {
    return depth_;
}

template <PriceType P>
Decimal PriceLevel<P>::volume() {
    return volume_;
}

template <PriceType P>
void PriceLevel<P>::append(Order* order) {
    auto it = price_tree_.find(order->price);
    auto q = &*it;
    if (it == price_tree_.end()) {
        q = queue_pool_.acquire(order->price);
        price_tree_.insert_equal(*q);
        ++depth_;
    }

    ++num_orders_;
    volume_ += order->qty;
    order->queue = q;
    q->append(order);
}

template <PriceType P>
void PriceLevel<P>::remove(Order* order) {
    auto q = order->queue;
    if (q != nullptr) {
        q->remove(order);
    }

    if (q->len() == 0) {
        price_tree_.erase(q->price());
        --depth_;
        queue_pool_.release(&*q);
    }

    --num_orders_;
    volume_ -= order->qty;
}

template <PriceType P>
OrderQueue* PriceLevel<P>::getQueue() {
    auto q = price_tree_.begin();
    if (q != price_tree_.end()) {
        return &*q;
    }

    return nullptr;
}

template <PriceType P>
template <PriceType Q>
Decimal PriceLevel<P>::processMarketOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, UserID takerUserID, Decimal qty, Flag flag) {
    static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");

    if ((flag & (AoN | FoK)) != 0) {
        // AoN/FoK must match the full requested quantity; exclude same-user (STP-skipped)
        // quantity so self-liquidity cannot cause the pre-check to falsely pass.
        Decimal availableVolume{};
        for (auto it = price_tree_.begin(); it != price_tree_.end(); ++it) {
            availableVolume += it->availableQty(takerUserID);
        }
        if (qty > availableVolume) {
            return Decimal{};
        }
    }

    auto qtyLeft = qty;
    Decimal qtyProcessed = uint64_t(0);
    for (auto q = getQueue(); !qtyLeft.is_zero() && q != nullptr;) {
        auto pq = q->process(tn, pf, takerOrderID, takerUserID, qtyLeft);
        if (!pq.is_zero()) {
            qtyLeft -= pq;
            qtyProcessed += pq;
            volume_ -= pq;
            q = getQueue();
        } else {
            // No progress at this price level (all STP); advance to next queue.
            q = getNextQueue<Q>(q->price());
        }
    }

    return qtyProcessed;
};

template <PriceType P>
template <PriceType Q>
Decimal PriceLevel<P>::processLimitOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, UserID takerUserID, Decimal price, Decimal qty, Flag flag) {
    static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");
    Decimal qtyProcessed = {};
    auto orderQueue = getQueue();

    if (orderQueue == nullptr) {
        return qtyProcessed;
    }

    if constexpr (std::is_same_v<CompareType, CmpGreater>) {
        if (price > orderQueue->price()) {
            return qtyProcessed;
        }
    } else {
        if (price < orderQueue->price()) {
            return qtyProcessed;
        }
    }

    // AoN/FoK pre-check: only continue when aggregate STP-aware fillable volume exists at eligible price levels.
    // Use availableQty (excludes same-user orders) so that self-liquidity cannot cause a false pass.
    if (flag & (AoN | FoK)) {
        if (qty > volume()) {
            return Decimal{};
        }

        bool canFill = false;
        auto aQty = qty;
        if constexpr (std::is_same_v<CompareType, CmpGreater>) {
            // Bid tree is descending (best = highest). Accumulate from bids >= sell limit.
            for (auto it = price_tree_.begin(); it != price_tree_.end() && it->price() >= price; ++it) {
                auto avail = it->availableQty(takerUserID);
                if (aQty <= avail) {
                    canFill = true;
                    break;
                }
                aQty -= avail;
            }
        } else {
            // Ask tree is ascending (best = lowest). Accumulate from asks <= buy limit.
            for (auto it = price_tree_.begin(); it != price_tree_.end() && it->price() <= price; ++it) {
                auto avail = it->availableQty(takerUserID);
                if (aQty <= avail) {
                    canFill = true;
                    break;
                }
                aQty -= avail;
            }
        }

        if (!canFill) {
            return Decimal{};
        }
    }

    orderQueue = getQueue();
    Decimal qtyLeft = qty;

    for (orderQueue = getQueue(); !qtyLeft.is_zero() && orderQueue != nullptr;) {
        // Stop as soon as the best remaining queue price no longer satisfies the limit.
        if constexpr (std::is_same_v<CompareType, CmpGreater>) {
            if (orderQueue->price() < price) break;  // bid price fell below sell limit
        } else {
            if (orderQueue->price() > price) break;  // ask price rose above buy limit
        }
        Decimal result = orderQueue->process(tn, pf, takerOrderID, takerUserID, qtyLeft);
        if (!result.is_zero()) {
            qtyLeft -= result;
            qtyProcessed += result;
            volume_ -= result;
            orderQueue = getQueue();
        } else {
            // No progress at this price level (all STP); advance to next queue.
            orderQueue = getNextQueue<Q>(orderQueue->price());
        }
    }

    return qtyProcessed;
};

template <PriceType P>
OrderQueue* PriceLevel<P>::largestLessThan(const Decimal& price) {
    auto it = price_tree_.lower_bound(price, PriceCompare());
    if (it != price_tree_.begin()) {
        --it;
        return &(*it);
    }
    return nullptr;
}

template <PriceType P>
OrderQueue* PriceLevel<P>::smallestGreaterThan(const Decimal& price) {
    auto it = price_tree_.upper_bound(price, PriceCompare());
    if (it != price_tree_.end()) {
        return &(*it);
    }
    return nullptr;
}

template <PriceType P>
template <PriceType Q>
OrderQueue* PriceLevel<P>::getNextQueue(const Decimal& price) {
    if constexpr (Q == PriceType::Bid) {
        return largestLessThan(price);
    } else if constexpr (Q == PriceType::Ask) {
        return smallestGreaterThan(price);
    } else {
        static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");
    }
}

template class PriceLevel<PriceType::Bid>;
template class PriceLevel<PriceType::Ask>;

template Decimal PriceLevel<PriceType::Bid>::processMarketOrder<PriceType::Bid>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                                UserID takerUserID, Decimal qty, Flag flag);

template Decimal PriceLevel<PriceType::Ask>::processMarketOrder<PriceType::Ask>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                                UserID takerUserID, Decimal qty, Flag flag);

template Decimal PriceLevel<PriceType::Bid>::processLimitOrder<PriceType::Bid>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                               UserID takerUserID, Decimal price, Decimal qty, Flag flag);

template Decimal PriceLevel<PriceType::Ask>::processLimitOrder<PriceType::Ask>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                               UserID takerUserID, Decimal price, Decimal qty, Flag flag);

}  // namespace orderbook
