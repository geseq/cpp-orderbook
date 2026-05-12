#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "boost/intrusive/list_hook.hpp"
#include "boost/intrusive/set_hook.hpp"
#include "types.hpp"

namespace orderbook {

class OrderQueue;

using namespace boost::intrusive;

struct Order : public set_base_hook<optimize_size<false>>, list_base_hook<constant_time_size<true>> {
    // ref_id is the caller's reference identifier, supplied at submission and
    // echoed back in all execution reports so the gateway can correlate fills
    // and cancels to its own order records.
    OrderID ref_id;

    // book_id is the engine's own monotonically-increasing identifier assigned
    // when the order enters the book.  It is never exposed to callers and is
    // used purely for internal tracking.
    BookOrderID book_id;

    Decimal qty;
    Decimal original_qty;
    Decimal price;
    Type type;
    Flag flag;
    Side side;

    // Stamped by the engine the moment this order (or re-slice) enters the
    // price-level queue.  This is the authoritative time-priority key: callers
    // never supply it, so no client can manipulate queue position.  For iceberg
    // orders each new visible slice receives a fresh queue_time when it is
    // re-appended, correctly placing it behind all orders already resting at
    // that price level.
    std::chrono::steady_clock::time_point queue_time{};

    OrderQueue *queue = nullptr;

    Order(OrderID ref_id, BookOrderID book_id, Type type, Side side, Decimal qty, Decimal price, Flag flag)
        : ref_id(ref_id), book_id(book_id), qty(qty), original_qty(qty), price(price), type(type), flag(flag), side(side){};

    friend bool operator<(const Order &a, const Order &b) { return a.ref_id < b.ref_id; }
    friend bool operator>(const Order &a, const Order &b) { return a.ref_id > b.ref_id; }
    friend bool operator==(const Order &a, const Order &b) { return a.ref_id == b.ref_id; }
};

struct OrderIDCompare {
    bool operator()(const Order &o1, const Order &o2) const { return o1.ref_id < o2.ref_id; }

    bool operator()(const Order &o, const OrderID &id) const { return o.ref_id < id; }

    bool operator()(const OrderID &id, const Order &o) const { return id < o.ref_id; }
};

}  // namespace orderbook

