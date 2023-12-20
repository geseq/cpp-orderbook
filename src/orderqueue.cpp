#include "orderqueue.hpp"

#include <boost/assert.hpp>
#include <cstdint>
#include <cwchar>
#include <iterator>
#include <memory>

#include "orderbook.hpp"

namespace orderbook {

Order* OrderQueue::head() const { return head_; }

Order* OrderQueue::tail() const { return tail_; }

Decimal OrderQueue::price() const { return price_; }

Decimal OrderQueue::totalQty() const { return total_qty_; }

uint64_t OrderQueue::len() { return size_; }

void OrderQueue::append(Order* order) {
    total_qty_ += order->qty;
    auto tail_temp = tail_;

    tail_ = order;
    if (tail_temp != nullptr) {
        tail_temp->next = order;
        order->prev = tail_temp;
    }

    if (head_ == nullptr) {
        head_ = order;
    }

    ++size_;
}

void OrderQueue::remove(Order* o) {
    total_qty_ = total_qty_ - o->qty;
    auto* prev = o->prev;
    auto* next = o->next;

    if (prev != nullptr) {
        prev->next = next;
    }

    if (next != nullptr) {
        next->prev = prev;
    }

    o->next = nullptr;
    o->prev = nullptr;

    --size_;

    if (head_ == o) {
        head_ = next;
    }

    if (tail_ == o) {
        tail_ = prev;
    }
}

Decimal OrderQueue::process(const TradeNotification& tradeNotification, const PostOrderFill& postFill, OrderID takerOrderID, Decimal qty) {
    Decimal qtyProcessed = {};
    BOOST_ASSERT(head_ != nullptr);
    for (auto ho = head_; ho != nullptr && qty > uint64_t(0); ho = head_) {
        if (qty < ho->qty) {
            qtyProcessed += qty;
            ho->qty -= qty;
            total_qty_ -= qty;
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledPartial, OrderStatus::FilledComplete, qty, ho->price);
            break;
        } else if (qty > ho->qty) {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            postFill(ho->id);
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledPartial, ho->qty, ho->price);
        } else {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            postFill(ho->id);
            tradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledComplete, ho->qty, ho->price);
        }
    }

    return qtyProcessed;
}

}  // namespace orderbook
