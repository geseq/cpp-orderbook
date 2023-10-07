#include "orderbook.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace orderbook {

using orderbook::Error;
using orderbook::Flag;
using orderbook::MsgType;
using orderbook::OrderStatus;
using orderbook::Side;
using orderbook::Type;

void OrderBook::addOrder(uint64_t tok, OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
    uint64_t exp = tok - 1;
    if (!last_token_.compare_exchange_strong(exp, tok)) {
        throw std::invalid_argument("invalid token received: cannot maintain determinism");
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
        if (orders_.find(id, OrderIDCompare()) != orders_.end()) {
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

void OrderBook::addTrigOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
    // TODO
    return;
}

void OrderBook::processOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    auto lp = last_price;
    // TODO defer

    if (type == Type::Market) {
        if (side == Side::Buy) {
        } else {
        }

        return;
    }

    Decimal qtyProcessed;
    if (side == Side::Buy) {
    } else {
    }

    if ((flag & (IoC | FoK)) != 0) {
        return;
    }

    // TODO
    return;
}

void OrderBook::putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {
    notification_.putTrade(mOrderID, tOrderID, mStatus, tStatus, qty, price);
}

std::shared_ptr<Order> OrderBook::cancelOrder(OrderID id) {
    if (orders_.find(id, orderbook::OrderIDCompare()) == 0) {
        // TODO cancel triger
        return nullptr;
    }

    auto it = orders_.find(id, OrderIDCompare());
    if (it != orders_.end()) {
        std::shared_ptr<Order> order(&*it, [](Order *ptr) { delete ptr; });
        orders_.erase(it);

        if (order->side == Side::Buy) {
            bids_.remove(order);
            return order;
        }

        asks_.remove(order);

        return order;
    }

    return nullptr;
}

}  // namespace orderbook
