#include "order.hpp"

#include <iostream>

namespace orderbook {

Decimal Order::getPrice(PriceType pt) {
    if (pt == PriceType::Trigger) {
        return trig_price;
    }

    return price;
}

void Order::release() {
    // TODO: put back in pool
}

}  // namespace orderbook
