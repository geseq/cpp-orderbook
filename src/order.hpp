#include <cstdint>
#include <memory>

#include "boost/intrusive/list_hook.hpp"
#include "boost/intrusive/set_hook.hpp"
#include "types.hpp"

namespace orderbook {

class OrderQueue;

struct Order : public boost::intrusive::set_base_hook<boost::intrusive::optimize_size<false>>, public boost::intrusive::list_base_hook<> {
    OrderId id;
    Decimal qty;
    Decimal price;
    Decimal trig_price;
    Type type;
    Flag flag;
    Side side;

    OrderQueue *queue = nullptr;

    Order(OrderId id, Decimal qty, Decimal price, Type type, Side side, Flag flag) : id(id), qty(qty), price(price), type(type), side(side), flag(flag){};

    Order(OrderId id, Decimal qty, Decimal price, Decimal trig_price, Type type, Side side, Flag flag)
        : id(id), qty(qty), price(price), trig_price(trig_price), type(type), side(side), flag(flag){};

    Decimal getPrice(PriceType pt);
    void release();

    friend bool operator<(const Order &a, const Order &b) { return a.id < b.id; }
    friend bool operator>(const Order &a, const Order &b) { return a.id > b.id; }
    friend bool operator==(const Order &a, const Order &b) { return a.id == b.id; }
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

