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

void OrderBook::addOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
    uint64_t exp = tok - 1;
    if (!last_token_.compare_exchange_strong(exp, tok)) {
        throw std::invalid_argument("invalid token received: cannot maintain determinism");
    }

    if (!matching_) {
        if (type == Market) {
            notification_.putOrder(MsgCreateOrder, OrderRejected, id, qty, ErrNoMatching);
        }

        if (side == Buy) {
            auto q = asks_.getQueue();
            if (q != nullptr && q->price() <= price) {
                notification_.putOrder(MsgCreateOrder, OrderRejected, id, qty, ErrNoMatching);
                return;
            }
        } else {
            auto q = bids_.getQueue();
            if (q != nullptr && q->price() >= price) {
                notification_.putOrder(MsgCreateOrder, OrderRejected, id, qty, ErrNoMatching);
                return;
            }
        }
    }

    if ((flag & (StopLoss | TakeProfit)) != 0) {
        if (trigPrice == udecimal::Zero) {
            notification_.putOrder(MsgCreateOrder, OrderRejected, id, qty, ErrInvalidTriggerPrice);
            return;
        }
        notification_.putOrder(MsgCreateOrder, OrderAccepted, id, qty);
        addTrigOrder(id, type, side, qty, price, trigPrice, flag);
        return;
    }

    if (type != Market) {
        if (orders_.count(id) > 0) {
            notification_.putOrder(MsgCreateOrder, OrderRejected, id, 0, ErrOrderExists);
            return;
        }

        if (price == 0) {
            notification_.putOrder(MsgCreateOrder, OrderRejected, id, 0, ErrInvalidPrice);
            return;
        }
    }

    notification_.putOrder(MsgCreateOrder, OrderAccepted, id, qty);
    processOrder(id, type, side, qty, price, flag);
}

void OrderBook::addTrigOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
    // TODO
    return;
}

void OrderBook::processOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    auto lp = last_price;
    // TODO defer

    if (type == Market) {
        if (side == Buy) {
        } else {
        }

        return;
    }

    Decimal qtyProcessed;
    if (side == Buy) {
    } else {
    }

    if ((flag & (IoC | FoK)) != 0) {
        return;
    }

    // TODO
    return;
}

void OrderBook::putTradeNotification(uint64_t mOrderId, uint64_t tOrderId, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {
    notification_.putTrade(mOrderId, tOrderId, mStatus, tStatus, qty, price);
}

std::shared_ptr<Order> OrderBook::cancelOrder(OrderId id) {
    if (orders_.count(id) == 0) {
        // TODO cancel triger
        return nullptr;
    }

    auto order = orders_[id];
    orders_.erase(id);

    if (order->side == Buy) {
        bids_.remove(order);
        return order;
    }

    asks_.remove(order);
    return order;
}

}  // namespace orderbook
