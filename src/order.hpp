#include <cstdint>
#include <memory>

#include "types.hpp"

namespace orderbook {

class OrderQueue;

struct Order {
    OrderId id;
    Decimal qty;
    Decimal price;
    Decimal trig_price;
    Type type;
    Flag flag;
    Side side;

    std::shared_ptr<Order> prev = nullptr;
    std::shared_ptr<Order> next = nullptr;
    std::shared_ptr<OrderQueue> queue = nullptr;

    Order(Decimal id, Decimal qty, Decimal price, Type type, Side side, Flag flag) : id(id), qty(qty), price(price), type(type), side(side), flag(flag) {
        trig_price = 0;
    };

    Order(OrderId id, Decimal qty, Decimal price, Decimal trig_price, Type type, Side side, Flag flag)
        : id(id), qty(qty), price(price), trig_price(trig_price), type(type), side(side), flag(flag){};

    Decimal getPrice(PriceType pt);
    void release();
};

Decimal Order::getPrice(PriceType pt) {
    if (pt == TrigPrice) {
        return trig_price;
    }

    return price;
}

void release() {
    // TODO: put back in pool
}

}  // namespace orderbook

