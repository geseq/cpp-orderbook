#include "pricelevel.hpp"

#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <map>
#include <memory>

namespace orderbook {

uint64_t PriceLevel::len() { return num_orders_; }

uint64_t PriceLevel::depth() { return depth_; }

Decimal PriceLevel::volume() { return volume_; }

void PriceLevel::append(const std::shared_ptr<Order>& order) {
    auto price = order.get()->getPrice(price_type_);

    if (price_tree_.count(price) == 0) {
        auto q = std::make_shared<OrderQueue>(price);
        price_tree_.insert(std::make_pair(price, q));
        ++depth_;
    }

    auto q = price_tree_[price];
    ++num_orders_;
    volume_ += order->qty;
    order->queue = q;
}

void PriceLevel::remove(const std::shared_ptr<Order>& order) {
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

std::shared_ptr<OrderQueue> PriceLevel::getQueue() {
    switch (price_type_) {
        case BidPrice:
            return getMaxPriceQueue();
        case AskPrice:
            return getMinPriceQueue();
        default:
            // TODO
            throw;
    }
}

std::shared_ptr<OrderQueue> PriceLevel::getMinPriceQueue() {
    if (depth_ > 0) {
        auto q = price_tree_.begin();
        if (q != price_tree_.end()) {
            return q->second;
        }
    }

    return nullptr;
}

std::shared_ptr<OrderQueue> PriceLevel::getMaxPriceQueue() {
    if (depth_ > 0) {
        auto q = price_tree_.rbegin();
        if (q != price_tree_.rend()) {
            return q->second;
        }
    }

    return nullptr;
}

Decimal PriceLevel::processMarketOrder(OrderBook* ob, OrderId takerOrderId, Decimal qty, Flag flag) {
    // TODO: this won't work as pricelevel volumes aren't accounted for correctly
    if ((flag & (AoN | FoK)) != 0 && qty > volume_) {
        return 0;
    }

    auto qtyLeft = qty;
    Decimal qtyProcessed = 0;
    for (auto q = getQueue(); qtyLeft > 0 && q != nullptr; q = getQueue()) {
        auto pq = q->process(ob, takerOrderId, qtyLeft);
        qtyLeft -= pq;
        qtyProcessed += pq;
    }

    return 0;
}

}  // namespace orderbook
