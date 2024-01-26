#pragma once

#include <cstdint>
#include <memory>

#include "boost/intrusive/list_hook.hpp"
#include "boost/intrusive/set_hook.hpp"
#include "types.hpp"

namespace orderbook {

class OrderQueue;

using namespace boost::intrusive;

struct Order : public set_base_hook<optimize_size<false>>, list_base_hook<constant_time_size<true>> {
    OrderID id;
    Decimal qty;
    Decimal price;
    Decimal trig_price;
    Type type;
    Flag flag;
    Side side;

    OrderQueue *queue = nullptr;

    Order(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) : id(id), qty(qty), price(price), type(type), side(side), flag(flag){};

    Order(OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trig_price, Flag flag)
        : id(id), qty(qty), price(price), trig_price(trig_price), type(type), side(side), flag(flag){};

    template <PriceType pt>
    Decimal getPrice() {
        if constexpr (pt == PriceType::TriggerOver || pt == PriceType::TriggerUnder) {
            return trig_price;
        }

        return price;
    }

    friend bool operator<(const Order &a, const Order &b) { return a.id < b.id; }
    friend bool operator>(const Order &a, const Order &b) { return a.id > b.id; }
    friend bool operator==(const Order &a, const Order &b) { return a.id == b.id; }
};

struct OrderIDCompare {
    bool operator()(const Order &o1, const Order &o2) const { return o1.id < o2.id; }

    bool operator()(const Order &o, const OrderID &id) const { return o.id < id; }

    bool operator()(const OrderID &id, const Order &o) const { return id < o.id; }
};

}  // namespace orderbook

