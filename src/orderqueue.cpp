#include "orderqueue.hpp"

#include <boost/assert.hpp>
#include <cstdint>
#include <cwchar>
#include <iterator>
#include <memory>

#include "orderbook.hpp"

namespace orderbook {

Decimal OrderQueue::price() const { return price_; }

Decimal OrderQueue::totalQty() const { return total_qty_; }

uint64_t OrderQueue::len() { return orders_.size(); }

void OrderQueue::append(Order* order) {
    total_qty_ += order->qty;
    orders_.push_back(*order);
}

void OrderQueue::remove(Order* o) {
    total_qty_ = total_qty_ - o->qty;
    auto it = orders_.iterator_to(*o);
    if (it != orders_.end()) {
        orders_.erase(it);
    }
}

Decimal OrderQueue::process(const TradeNotification& tradeNotification, const PostOrderFill& postFill, OrderID takerOrderID, Decimal qty) {
    Decimal qtyProcessed = {};
    BOOST_ASSERT(orders_.begin() != orders_.end());
    for (auto it = orders_.begin(); it != orders_.end() && qty > uint64_t(0);) {
        BOOST_ASSERT(it != orders_.end());
        BOOST_ASSERT(orders_.begin() != orders_.end());
        auto* ho = &*it;
        BOOST_ASSERT(ho != nullptr);
        if (qty < ho->qty) {
            qtyProcessed += qty;
            ho->qty -= qty;
            total_qty_ -= qty;
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledPartial, OrderStatus::FilledComplete, qty, ho->price);
            break;
        } else if (qty > ho->qty) {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            ++it;
            postFill(ho->id);
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledPartial, ho->qty, ho->price);
        } else {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            ++it;
            postFill(ho->id);
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledComplete, ho->qty, ho->price);
        }
    }

    return qtyProcessed;
}

}  // namespace orderbook
