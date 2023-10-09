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

std::shared_ptr<Order> OrderQueue::remove(const std::shared_ptr<Order>& o) {
    total_qty_ = total_qty_ - o->qty;
    auto prev = o->prev;
    auto next = o->next;

    if (prev != nullptr) {
        prev->next = next;
    }

    if (next != nullptr) {
        next->prev = prev;
    }

    o->next = nullptr;
    o->prev = nullptr;

    --size_;

    if (head_ == o.get()) {
        head_ = next;
    }

    if (tail_ == o.get()) {
        tail_ = prev;
    }

    return o;
}

Decimal OrderQueue::process(OrderBook* ob, OrderID takerOrderID, Decimal qty) {
    Decimal qtyProcessed = {};
    BOOST_ASSERT(head_ != nullptr);
    for (auto ho = head_; ho != nullptr && qty > uint64_t(0); ho = head_) {
        if (qty < ho->qty) {
            qtyProcessed += qty;
            ho->qty -= qty;
            total_qty_ -= qty;
            ob->putTradeNotification(ho->id, takerOrderID, OrderStatus::FilledPartial, OrderStatus::FilledComplete, qty, ho->price);
            ob->last_price = ho->price;
            break;
        } else if (qty > ho->qty) {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            ob->cancelOrder(ho->id);
            ob->putTradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledPartial, ho->qty, ho->price);
            ob->last_price = ho->price;
            ho->release();
        } else {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            ob->cancelOrder(ho->id);
            ob->putTradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledComplete, ho->qty, ho->price);
            ob->last_price = ho->price;
            ho->release();
        }
    }

    return qtyProcessed;
}

}  // namespace orderbook
