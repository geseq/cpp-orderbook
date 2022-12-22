#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <map>
#include <memory>

#include "orderqueue.hpp"

namespace orderbook {

class PriceLevel {
    std::map<Decimal, std::shared_ptr<OrderQueue>> price_tree_;

    PriceType price_type_;
    Decimal volume_ = 0;
    uint64_t num_orders_ = 0;
    uint64_t depth_ = 0;

   public:
    PriceLevel(PriceType price_type) : price_type_(price_type){};
    uint64_t len();
    uint64_t depth();
    Decimal volume();
    void append(const std::shared_ptr<Order>&  order);
    void remove(const std::shared_ptr<Order>&  order);
};

uint64_t PriceLevel::len() { return num_orders_; }

uint64_t PriceLevel::depth() { return depth_; }

Decimal PriceLevel::volume() { return volume_; }

void PriceLevel::append(const std::shared_ptr<Order>&  order) {
    auto price = order.get()->getPrice(price_type_);

    if (price_tree_.count(price) == 0) {
        auto q = std::make_shared<OrderQueue>(price);
        price_tree_.insert(std::make_pair(price, q));
        ++depth_;
    }

    auto q = price_tree_[price];
    ++num_orders_;
    volume_ += order->getQty();
    order->queue_ = q;
}

void PriceLevel::remove(const std::shared_ptr<Order>&  order) {
    auto price = order.get()->getPrice(price_type_);
    
    auto q = order->queue_;
    if (q != nullptr) {
        q->remove(order);
    }

    if (q->len() == 0) {
        price_tree_.erase(price);
        --depth_;
    }

    --num_orders_;
    volume_ -= order->getQty();
}

}  // namespace orderbook

