#include "orderbook.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "util.hpp"

namespace orderbook {

using orderbook::Error;
using orderbook::Flag;
using orderbook::MsgType;
using orderbook::OrderStatus;
using orderbook::Side;
using orderbook::Type;

void OrderBook::addOrder(uint64_t tok, OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
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

void OrderBook::addTrigOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
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

void OrderBook::postProcess(Decimal& lp) {
    if (lp == last_price) {
        return;
    }

    queueTriggeredOrders();
    processTriggeredOrders();
}

void OrderBook::queueTriggeredOrders() {}

void OrderBook::processTriggeredOrders() {}

void OrderBook::processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
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

void OrderBook::putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {
    notification_.putTrade(mOrderID, tOrderID, mStatus, tStatus, qty, price);
}

void OrderBook::cancelOrder(uint64_t tok, OrderID id) {
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

std::shared_ptr<Order> OrderBook::cancelOrder(OrderID id) {
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

bool OrderBook::hasOrder(OrderID id) { return orders_.find(id, OrderIDCompare()) != orders_.end(); }

std::string OrderBook::toString() {
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
