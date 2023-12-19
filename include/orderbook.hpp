#pragma once

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
          bids_(PriceLevel<CmpGreater>(PriceType::Bid, price_level_pool_size)),
          asks_(PriceLevel<CmpLess>(PriceType::Ask, price_level_pool_size)),
          trigger_over_(PriceLevel<CmpGreater>(PriceType::Trigger, price_level_pool_size)),
          trigger_under_(PriceLevel<CmpLess>(PriceType::Trigger, price_level_pool_size)),
          orders_(OrderMap()),
          trig_orders_(OrderMap()){};

    void addOrder(uint64_t tok, OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    void cancelOrder(uint64_t tok, OrderID id);
    bool hasOrder(OrderID id);

    std::string toString();

    Decimal last_price;

   private:
    pool::AdaptiveObjectPool<Order> order_pool_;

    PriceLevel<CmpGreater> bids_;
    PriceLevel<CmpLess> asks_;
    PriceLevel<CmpGreater> trigger_over_;
    PriceLevel<CmpLess> trigger_under_;

    OrderMap orders_;
    OrderMap trig_orders_;

    Notification& notification_;

    std::atomic_uint64_t last_token_ = 0;

    std::atomic_uint64_t matching_ = 1;

    std::shared_ptr<Order> cancelOrder(OrderID id);
    void addTrigOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
    void postProcess(Decimal& lp);
    void queueTriggeredOrders();
    void processTriggeredOrders();
};

template <class Notification>
void OrderBook<Notification>::addOrder(uint64_t tok, OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
    uint64_t exp = tok - 1;  // technically this should always be single threaded, but just in case.
    if (!last_token_.compare_exchange_strong(exp, tok)) {
        throw std::invalid_argument("invalid token received: cannot maintain determinism");
    }

    if (qty.is_zero()) {
        notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::InvalidQty);
        return;
    }

    if (!matching_) {
        if (type == Type::Market) {
            notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::NoMatching);
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

    if ((flag & (Flag::StopLoss | Flag::TakeProfit)) != 0) {
        if (trigPrice.is_zero()) {
            notification_.putOrder(MsgType::CreateOrder, OrderStatus::Rejected, id, qty, Error::InvalidTriggerPrice);
            return;
        }
        notification_.putOrder(MsgType::CreateOrder, OrderStatus::Accepted, id, qty);
        addTrigOrder(id, type, side, qty, price, trigPrice, flag);
        return;
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
void OrderBook<Notification>::addTrigOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
    // TODO
    return;

    switch (flag) {
        case Flag::StopLoss:
            switch (side) {
                case Side::Buy:
                    if (trigPrice <= last_price) {
                        processOrder(id, type, side, qty, price, flag);
                        return;
                    }
                    {
                        auto* o = order_pool_.acquire(id, type, side, qty, price, trigPrice, flag);
                        trigger_over_.append(o);
                        trig_orders_.insert_equal(*o);
                    }
                    break;
                case Side::Sell:
                    if (last_price <= trigPrice) {
                        processOrder(id, type, side, qty, price, flag);
                        return;
                    }
                    {
                        auto* o = order_pool_.acquire(id, type, side, qty, price, trigPrice, flag);
                        trigger_under_.append(o);
                        trig_orders_.insert_equal(*o);
                    }
                    break;
            }
            break;
        case Flag::TakeProfit:
            switch (side) {
                case Side::Buy:
                    if (last_price <= trigPrice) {
                        processOrder(id, type, side, qty, price, flag);
                        return;
                    }
                    {
                        auto* o = order_pool_.acquire(id, type, side, qty, price, trigPrice, flag);
                        trigger_under_.append(o);
                        trig_orders_.insert_equal(*o);
                    }
                    break;
                case Side::Sell:
                    if (trigPrice <= last_price) {
                        processOrder(id, type, side, qty, price, flag);
                        return;
                    }
                    {
                        auto* o = order_pool_.acquire(id, type, side, qty, price, trigPrice, flag);
                        trigger_over_.append(o);
                        trig_orders_.insert_equal(*o);
                    }
                    break;
            }
            break;
        default:
            break;
    }
}

template <class Notification>
void OrderBook<Notification>::postProcess(Decimal& lp) {
    if (lp == last_price) {
        return;
    }

    queueTriggeredOrders();
    processTriggeredOrders();
}

template <class Notification>
void OrderBook<Notification>::queueTriggeredOrders() {}

template <class Notification>
void OrderBook<Notification>::processTriggeredOrders() {}

template <class Notification>
void OrderBook<Notification>::processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    auto lp = last_price;
    scope_exit defer([this, &lp]() { postProcess(lp); });

    static const auto tradeNotification = [this](OrderID mOrderID, OrderID tOrderID, OrderStatus mOrderStatus, OrderStatus tOrderStatus, Decimal qty,
                                                 Decimal price) {
        this->putTradeNotification(mOrderID, tOrderID, mOrderStatus, tOrderStatus, qty, price);
        this->last_price = price;
    };
    static const auto postOrderFill = [this](OrderID id) { this->cancelOrder(id); };

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
        auto* o = order_pool_.acquire(id, type, side, qtyLeft, price, uint64_t(0), flag);
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
void OrderBook<Notification>::cancelOrder(uint64_t tok, OrderID id) {
    uint64_t exp = tok - 1;
    if (!last_token_.compare_exchange_strong(exp, tok)) {
        throw std::invalid_argument("invalid token received: cannot maintain determinism");
    }

    auto order = cancelOrder(id);
    if (order == nullptr) {
        notification_.putOrder(MsgType::CancelOrder, OrderStatus::Rejected, id, uint64_t(0), Error::OrderNotExists);
        return;
    }

    notification_.putOrder(MsgType::CancelOrder, OrderStatus::Canceled, id, order->qty);
    order->release();
}

template <class Notification>
std::shared_ptr<Order> OrderBook<Notification>::cancelOrder(OrderID id) {
    if (orders_.find(id, orderbook::OrderIDCompare()) == orders_.end()) {
        // TODO cancel triger
        return nullptr;
    }

    auto it = orders_.find(id, OrderIDCompare());
    if (it != orders_.end()) {
        auto& pool = order_pool_;
        std::shared_ptr<Order> order(&*it, [&pool](Order* ptr) { pool.release(ptr); });
        orders_.erase(*it);

        if (order->side == Side::Buy) {
            bids_.remove(order);
            return order;
        }

        asks_.remove(order);

        return order;
    }

    return nullptr;
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
            ss << b->totalQty().to_string() << "\t" << b->price().to_string();
            ++b;
        } else {
            ss << "\t\t\t";
        }

        ss << " | ";
        if (a != asks.end()) {
            ss << a->price().to_string() << "\t" << a->totalQty().to_string();
            ++a;
        }

        loop = (b != bids.end() || a != asks.end());
        ss << std::endl;
    }

    return ss.str();
}

}  // namespace orderbook
