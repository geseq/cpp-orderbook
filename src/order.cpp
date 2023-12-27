#include "order.hpp"

#include <iostream>

#include "types.hpp"

namespace orderbook {

template <PriceType pt>
Decimal Order::getPrice() {
    if constexpr (pt == PriceType::TriggerOver || pt == PriceType::TriggerUnder) {
        return trig_price;
    }

    return price;
}

}  // namespace orderbook
