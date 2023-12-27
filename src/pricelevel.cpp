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
    auto price = order->getPrice<P>();

    auto it = price_tree_.find(price);
    auto q = &*it;
    if (it == price_tree_.end()) {
        q = queue_pool_.acquire(price);
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
    auto price = order->getPrice<P>();

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
Decimal PriceLevel<P>::processMarketOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal qty, Flag flag) {
    static_assert(Q == PriceType::Bid || Q == PriceType::Ask, "Unsupported PriceType");
    // TODO: this won't work as pricelevel volumes aren't accounted for correctly
    if ((flag & (AoN | FoK)) != 0 && qty > volume_) {
        return uint64_t(0);
    }

    auto qtyLeft = qty;
    Decimal qtyProcessed = uint64_t(0);
    for (auto q = getQueue(); !qtyLeft.is_zero() && q != nullptr; q = getQueue()) {
        auto pq = q->process(tn, pf, takerOrderID, qtyLeft);
        qtyLeft -= pq;
        qtyProcessed += pq;
    }

    return uint64_t(0);
};

template <PriceType P>
template <PriceType Q>
Decimal PriceLevel<P>::processLimitOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal price, Decimal qty, Flag flag) {
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

    // TODO: Fix AoN
    if (flag & (AoN | FoK)) {
        if (qty > volume()) {
            return Decimal{};
        }

        bool canFill = false;
        auto aQty = qty;
        if constexpr (std::is_same_v<CompareType, CmpGreater>) {
            while (orderQueue != nullptr && price < orderQueue->price()) {
                if (aQty <= orderQueue->totalQty()) {
                    canFill = true;
                    break;
                }
                aQty -= orderQueue->totalQty();
                orderQueue = getNextQueue(orderQueue->price());
            }
        } else {
            while (orderQueue != nullptr && price > orderQueue->price()) {
                if (aQty <= orderQueue->totalQty()) {
                    canFill = true;
                    break;
                }
                aQty -= orderQueue->totalQty();
                orderQueue = getNextQueue(orderQueue->price());
            }
        }

        if (!canFill) {
            return Decimal{};
        }
    }

    orderQueue = getQueue();
    Decimal qtyLeft = qty;

    for (orderQueue = getQueue(); !qtyLeft.is_zero() && orderQueue != nullptr; orderQueue = getQueue()) {
        Decimal result = orderQueue->process(tn, pf, takerOrderID, qtyLeft);
        qtyLeft -= result;
        qtyProcessed += result;
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
template class PriceLevel<PriceType::TriggerOver>;
template class PriceLevel<PriceType::TriggerUnder>;

template Decimal PriceLevel<PriceType::Bid>::processMarketOrder<PriceType::Bid>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                                Decimal qty, Flag flag);

template Decimal PriceLevel<PriceType::Ask>::processMarketOrder<PriceType::Ask>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                                Decimal qty, Flag flag);

template Decimal PriceLevel<PriceType::Bid>::processLimitOrder<PriceType::Bid>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                               Decimal price, Decimal qty, Flag flag);

template Decimal PriceLevel<PriceType::Ask>::processLimitOrder<PriceType::Ask>(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID,
                                                                               Decimal price, Decimal qty, Flag flag);

}  // namespace orderbook
