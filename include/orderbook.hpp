#pragma once

#include <cstdint>
#include <map>
#include <sstream>

#include "boost/intrusive/rbtree.hpp"
#include "pricelevel.hpp"
#include "types.hpp"
#include "util.hpp"

namespace orderbook {

using OrderMap = boost::intrusive::rbtree<Order, CmpLess>;

template <class Notification>
class OrderBook {
   public:
    OrderBook(NotificationInterface<Notification>& n, size_t price_level_pool_size = 16384, size_t order_pool_size = 16384)
        : order_pool_(order_pool_size),
          notification_(static_cast<Notification&>(n)),
          bids_(PriceLevel<PriceType::Bid>(price_level_pool_size)),
          asks_(PriceLevel<PriceType::Ask>(price_level_pool_size)),
          orders_(OrderMap()){};

    void addOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
    void putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    void cancelOrder(OrderID id);
    bool hasOrder(OrderID id);
    void setMatching(bool matching) { matching_ = matching; }

    std::string toString();

    Decimal last_price;

   private:
    pool::AdaptiveObjectPool<Order> order_pool_;

    PriceLevel<PriceType::Bid> bids_;
    PriceLevel<PriceType::Ask> asks_;

    OrderMap orders_;

    Notification& notification_;

    bool matching_ = true;

    Decimal eraseOrder(OrderID id);
    void processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
};

template <class Notification>
void OrderBook<Notification>::addOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    if (qty.is_zero()) [[unlikely]] {
        notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::InvalidQty);
        return;
    }

    if (!matching_) [[unlikely]] {
        if (type == Type::Market) {
            notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::NoMatching);
            return;
        }

        if (side == Side::Buy) {
            auto q = asks_.getQueue();
            if (q != nullptr && q->price() <= price) {
                notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::NoMatching);
                return;
            }
        } else {
            auto q = bids_.getQueue();
            if (q != nullptr && q->price() >= price) {
                notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::NoMatching);
                return;
            }
        }
    }

    if (type != Type::Market) {
        if (orders_.find(id, orderbook::OrderIDCompare()) != orders_.end()) {
            notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, uint64_t(0), Error::OrderExists);
            return;
        }

        if (price.is_zero()) {
            notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, uint64_t(0), Error::InvalidPrice);
            return;
        }
    }

    notification_.putOrder(MsgType::CreateOrder, OrderStatus::Accepted, id, qty);
    processOrder(id, type, side, qty, price, flag);
}

template <class Notification>
void OrderBook<Notification>::processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    static const auto tradeNotification = [this](OrderID mOrderID, OrderID tOrderID, OrderStatus mOrderStatus, OrderStatus tOrderStatus, Decimal qty,
                                                 Decimal price) {
        this->putTradeNotification(mOrderID, tOrderID, mOrderStatus, tOrderStatus, qty, price);
        this->last_price = price;
    };
    static const auto postOrderFill = [this](OrderID id) { this->eraseOrder(id); };

    if (type == Type::Market) {
        if (side == Side::Buy) {
            asks_.processMarketOrder(tradeNotification, postOrderFill, id, qty, flag);
        } else {
            bids_.processMarketOrder(tradeNotification, postOrderFill, id, qty, flag);
        }

        return;
    }

    Decimal qtyProcessed;
    if (side == Side::Buy) {
        qtyProcessed = asks_.processLimitOrder(tradeNotification, postOrderFill, id, price, qty, flag);
    } else {
        qtyProcessed = bids_.processLimitOrder(tradeNotification, postOrderFill, id, price, qty, flag);
    }

    if ((flag & (IoC | FoK)) != 0) {
        return;
    }

    auto qtyLeft = qty - qtyProcessed;
    if (qtyLeft > uint64_t(0)) {
        auto* o = order_pool_.acquire(id, type, side, qtyLeft, price, flag);
        o->original_qty = qty;
        if (side == Side::Buy) {
            bids_.append(o);
        } else {
            asks_.append(o);
        }

        orders_.insert_equal(*o);
    }

    return;
}

template <class Notification>
void OrderBook<Notification>::putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {
    notification_.putTrade(mOrderID, tOrderID, mStatus, tStatus, qty, price);
}

template <class Notification>
void OrderBook<Notification>::cancelOrder(OrderID id) {
    auto qty = eraseOrder(id);
    if (qty.is_zero()) {
        notification_.putOrder(MsgType::CancelOrder, OrderStatus::Rejected, id, uint64_t(0), Error::OrderNotExists);
        return;
    }

    notification_.putOrder(MsgType::CancelOrder, OrderStatus::Canceled, id, qty);
}

template <class Notification>
Decimal OrderBook<Notification>::eraseOrder(OrderID id) {
    auto it = orders_.find(id, OrderIDCompare());
    if (it == orders_.end()) {
        return uint64_t(0);
    }

    auto& pool = order_pool_;
    auto* order = &*it;
    scope_exit defer([&pool, &order]() { pool.release(order); });
    orders_.erase(*it);

    if (order->side == Side::Buy) {
        bids_.remove(order);
        return order->qty;
    }

    asks_.remove(order);
    return order->qty;
}

template <class Notification>
bool OrderBook<Notification>::hasOrder(OrderID id) {
    return orders_.find(id, OrderIDCompare()) != orders_.end();
}

template <class Notification>
std::string OrderBook<Notification>::toString() {
    std::stringstream ss;

    const auto& bids = bids_.price_tree();
    const auto& asks = asks_.price_tree();

    auto b = bids.begin();
    auto a = asks.begin();

    auto loop = (b != bids.end() || a != asks.end());
    while (loop) {
        if (b != bids.end()) {
            ss << b->totalQty() << "\t" << b->price();
            ++b;
        } else {
            ss << "\t\t\t";
        }

        ss << " | ";
        if (a != asks.end()) {
            ss << a->price() << "\t" << a->totalQty();
            ++a;
        }

        loop = (b != bids.end() || a != asks.end());
        ss << std::endl;
    }

    return ss.str();
}

}  // namespace orderbook
