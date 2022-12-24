#include <cstdint>
#include <iterator>
#include <memory>

#include "order.hpp"

namespace orderbook {

class OrderQueue {
    std::shared_ptr<Order> head_ = nullptr;
    std::shared_ptr<Order> tail_ = nullptr;

    Decimal price_ = 0;
    Decimal total_qty_ = 0;
    uint64_t size_ = 0;

   public:
    OrderQueue(Decimal price) : price_(price){};
    auto price();
    auto len();
    auto totalQty();
    auto head();
    auto append(std::shared_ptr<Order> o);
    auto remove(const std::shared_ptr<Order>& o);
};

auto OrderQueue::price() { return price_; }

auto OrderQueue::len() {return size_;}

auto OrderQueue::remove(const std::shared_ptr<Order>& o) {
    total_qty_ -= o->qty;
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

}  // namespace orderbook
