#include "order.hpp"

#include <iostream>

namespace orderbook {

Decimal Order::getPrice(PriceType pt) {
    if (pt == PriceType::TriggerOver || pt == PriceType::TriggerUnder) [[unlikely]] {
        return trig_price;
    }

    return price;
}

}  // namespace orderbook
