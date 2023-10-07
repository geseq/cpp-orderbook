#include "orderqueue.hpp"

#include <cstdint>
#include <cwchar>
#include <iterator>
#include <memory>

#include "orderbook.hpp"

namespace orderbook {

Decimal OrderQueue::price() { return price_; }

uint64_t OrderQueue::len() { return size_; }

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

    if (head_ == o) {
        head_ = next;
    }

    if (tail_ == o) {
        tail_ = prev;
    }

    return o;
}

Decimal OrderQueue::process(OrderBook* ob, OrderID takerOrderID, Decimal qty) {
    Decimal qtyProcessed = {};
    for (auto ho = head_; ho != nullptr && qty > uint64_t(0); ho = head_) {
        if (qty < ho->qty) {
            qtyProcessed += qty;
            ho->qty -= qty;
            ob->putTradeNotification(ho->id, takerOrderID, OrderStatus::FilledParial, OrderStatus::FilledComplete, qty, ho->price);
            ob->last_price = ho->price;
        } else if (qty > ho->qty) {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            ob->putTradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledParial, ho->qty, ho->price);
            ob->last_price = ho->price;
        } else {
            qtyProcessed += ho->qty;
            qty -= ho->qty;
            ob->cancelOrder(ho->id);
            ob->putTradeNotification(ho->id, takerOrderID, OrderStatus::FilledComplete, OrderStatus::FilledComplete, ho->qty, ho->price);
            ho->release();
        }
    }

    return qtyProcessed;
}

}  // namespace orderbook
