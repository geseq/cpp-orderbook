#include "pricelevel.hpp"

#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <map>
#include <memory>

namespace orderbook {

template <class CompareType>
uint64_t PriceLevel<CompareType>::len() {
    return num_orders_;
}

template <class CompareType>
uint64_t PriceLevel<CompareType>::depth() {
    return depth_;
}

template <class CompareType>
Decimal PriceLevel<CompareType>::volume() {
    return volume_;
}

template <class CompareType>
void PriceLevel<CompareType>::append(const std::shared_ptr<Order>& order) {
    auto price = order.get()->getPrice(price_type_);

    if (price_tree_.count(price) == 0) {
        auto q = OrderQueue(price);
        price_tree_.insert_equal(q);
        ++depth_;
    }

    auto it = price_tree_.find(price, PriceCompare());
    if (it != price_tree_.end()) {
        ++num_orders_;
        volume_ += order->qty;
        order->queue = &*it;
    }
}

template <class CompareType>
void PriceLevel<CompareType>::remove(const std::shared_ptr<Order>& order) {
    auto price = order.get()->getPrice(price_type_);

    auto q = order->queue;
    if (q != nullptr) {
        q->remove(order);
    }

    if (q->len() == 0) {
        price_tree_.erase(price);
        --depth_;
    }

    --num_orders_;
    volume_ -= order->qty;
}

template <class CompareType>
OrderQueue* PriceLevel<CompareType>::getQueue() {
    auto q = price_tree_.begin();
    if (q != price_tree_.end()) {
        return &*q;
    }

    return nullptr;
}

template <class CompareType>
Decimal PriceLevel<CompareType>::processMarketOrder(OrderBook* ob, OrderID takerOrderID, Decimal qty, Flag flag) {
    // TODO: this won't work as pricelevel volumes aren't accounted for correctly
    if ((flag & (AoN | FoK)) != 0 && qty > volume_) {
        return uint64_t(0);
    }

    auto qtyLeft = qty;
    Decimal qtyProcessed = uint64_t(0);
    for (auto q = getQueue(); qtyLeft > uint64_t(0) && q != nullptr; q = getQueue()) {
        auto pq = q->process(ob, takerOrderID, qtyLeft);
        qtyLeft -= pq;
        qtyProcessed += pq;
    }

    return uint64_t(0);
};

template class PriceLevel<CmpGreater>;
template class PriceLevel<CmpLess>;

}  // namespace orderbook
