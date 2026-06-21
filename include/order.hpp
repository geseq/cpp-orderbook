#pragma once

#include <cstdint>
#include <memory>

#include "boost/intrusive/list_hook.hpp"
#include "types.hpp"

namespace orderbook {

class OrderQueue;

using namespace boost::intrusive;

struct Order : public list_base_hook<constant_time_size<true>> {
    OrderID id;
    Decimal qty;
    Decimal original_qty;
    Decimal price;
    Type type;
    Flag flag;
    Side side;

    OrderQueue *queue = nullptr;

    Order(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) : id(id), qty(qty), original_qty(qty), price(price), type(type), flag(flag), side(side){};

    friend bool operator<(const Order &a, const Order &b) { return a.id < b.id; }
    friend bool operator>(const Order &a, const Order &b) { return a.id > b.id; }
    friend bool operator==(const Order &a, const Order &b) { return a.id == b.id; }
};

}  // namespace orderbook

