#include "pricelevel.hpp"

#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <map>
#include <memory>
#include <string>

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
    auto price = order->getPrice(price_type_);

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
    auto price = order->getPrice(price_type_);

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
Decimal PriceLevel<P>::processMarketOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID takerOrderID, Decimal qty, Flag flag) {
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
Decimal PriceLevel<P>::processLimitOrder(const TradeNotification& tn, const PostOrderFill& pf, OrderID& takerOrderID, Decimal& price, Decimal qty, Flag& flag) {
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
OrderQueue* PriceLevel<P>::getNextQueue(const Decimal& price) {
    switch (price_type_) {
        case PriceType::Bid:
            return largestLessThan(price);
        case PriceType::Ask:
            return smallestGreaterThan(price);
        default:
            throw std::runtime_error("invalid call to GetQueue");
    }
}

template class PriceLevel<PriceType::Bid>;
template class PriceLevel<PriceType::Ask>;
template class PriceLevel<PriceType::TriggerOver>;
template class PriceLevel<PriceType::TriggerUnder>;

}  // namespace orderbook
