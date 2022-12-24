#include <cstdint>
#include <memory>

#include "types.hpp"

namespace orderbook {

class OrderQueue;
class PriceLevel;

struct Order {
    OrderId id;
    Decimal qty;
    Decimal price;
    Decimal trig_price;
    Type type;
    Flag flag;

    std::shared_ptr<Order> prev = nullptr;
    std::shared_ptr<Order> next = nullptr;
    std::shared_ptr<OrderQueue> queue = nullptr;

    Order(Decimal id, Decimal qty, Decimal price, Type type, Flag flag) : id(id), qty(qty), price(price), type(type), flag(flag) { trig_price = 0; };

    Order(OrderId id, Decimal qty, Decimal price, Decimal trig_price, Type type, Flag flag)
        : id(id), qty(qty), price(price), trig_price(trig_price), type(type), flag(flag){};

    Decimal getPrice(PriceType pt);
    Decimal getQty();
};

Decimal Order::getPrice(PriceType pt) {
    if (pt == TrigPrice) {
        return trig_price;
    }

    return price;
}

Decimal Order::getQty() { return qty; }

}  // namespace orderbook

