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
        } else {
            auto matchedQty = ho->qty;
            qtyProcessed += matchedQty;
            qty -= matchedQty;
            total_qty_ -= matchedQty;
            ++it;
            // Zero maker quantity before postFill so callbacks/removal observe a fully filled maker order.
            ho->qty = Decimal{};
            postFill(ho->id);
            // qty has already been decremented by matchedQty, so zero means taker is fully filled.
            const auto takerStatus = qty.is_zero() ? OrderStatus::FilledComplete : OrderStatus::FilledPartial;
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, takerStatus, matchedQty, ho->price);
        }
    }

    return qtyProcessed;
}

}  // namespace orderbook
