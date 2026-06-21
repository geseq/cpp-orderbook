#pragma once

#include <cstdint>
#include <sstream>
#include <utility>

#include "object_pool.hpp"
#include "order_index.hpp"
#include "pricelevel.hpp"
#include "types.hpp"
#include "util.hpp"

namespace orderbook {

using OrderMap = index::FibHashIndex;

template <class Notification>
class OrderBook {
   public:
    OrderBook(NotificationInterface<Notification>& n, size_t price_level_pool_size = 16384, size_t order_pool_size = 16384, size_t order_index_reserve = 16384)
        : order_pool_(order_pool_size),
          notification_(static_cast<Notification&>(n)),
          bids_(PriceLevel<PriceType::Bid>(price_level_pool_size)),
          asks_(PriceLevel<PriceType::Ask>(price_level_pool_size)),
          orders_(order_index_reserve) {};

    void addOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
    void putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    void cancelOrder(OrderID id);
    bool hasOrder(OrderID id);
    void setMatching(bool matching) { matching_ = matching; }

    std::string toString();

    Decimal last_price;

   private:
    pool::ObjectPool<Order> order_pool_;

    PriceLevel<PriceType::Bid> bids_;
    PriceLevel<PriceType::Ask> asks_;

    OrderMap orders_;

    Notification& notification_;

    bool matching_ = true;

    std::pair<Decimal, Decimal> eraseOrder(OrderID id);
    void processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
};

template <class Notification>
void OrderBook<Notification>::addOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    if (qty.is_zero()) [[unlikely]] {
        notification_.onExecutionReport(ExecutionReport{
            .exec_type = ExecType::Rejected,
            .msg_type = MsgType::CreateOrder,
            .order_id = id,
            .status = OrderStatus::Rejected,
            .qty = qty,
            .original_qty = qty,
            .error = Error::InvalidQty,
        });
        return;
    }

    if (!matching_) [[unlikely]] {
        if (type == Type::Market) {
            notification_.onExecutionReport(ExecutionReport{
                .exec_type = ExecType::Rejected,
                .msg_type = MsgType::CreateOrder,
                .order_id = id,
                .status = OrderStatus::Rejected,
                .qty = qty,
                .original_qty = qty,
                .error = Error::NoMatching,
            });
            return;
        }

        if (side == Side::Buy) {
            auto q = asks_.getQueue();
            if (q != nullptr && q->price() <= price) {
                notification_.onExecutionReport(ExecutionReport{
                    .exec_type = ExecType::Rejected,
                    .msg_type = MsgType::CreateOrder,
                    .order_id = id,
                    .status = OrderStatus::Rejected,
                    .qty = qty,
                    .original_qty = qty,
                    .error = Error::NoMatching,
                });
                return;
            }
        } else {
            auto q = bids_.getQueue();
            if (q != nullptr && q->price() >= price) {
                notification_.onExecutionReport(ExecutionReport{
                    .exec_type = ExecType::Rejected,
                    .msg_type = MsgType::CreateOrder,
                    .order_id = id,
                    .status = OrderStatus::Rejected,
                    .qty = qty,
                    .original_qty = qty,
                    .error = Error::NoMatching,
                });
                return;
            }
        }
    }

    if (type != Type::Market) {
        if (orders_.contains(id)) {
            notification_.onExecutionReport(ExecutionReport{
                .exec_type = ExecType::Rejected,
                .msg_type = MsgType::CreateOrder,
                .order_id = id,
                .status = OrderStatus::Rejected,
                .qty = uint64_t(0),
                .original_qty = qty,
                .error = Error::OrderExists,
            });
            return;
        }

        if (price.is_zero()) {
            notification_.onExecutionReport(ExecutionReport{
                .exec_type = ExecType::Rejected,
                .msg_type = MsgType::CreateOrder,
                .order_id = id,
                .status = OrderStatus::Rejected,
                .qty = uint64_t(0),
                .original_qty = qty,
                .error = Error::InvalidPrice,
            });
            return;
        }
    }

    notification_.onExecutionReport(ExecutionReport{
        .exec_type = ExecType::New,
        .msg_type = MsgType::CreateOrder,
        .order_id = id,
        .status = OrderStatus::Accepted,
        .qty = qty,
        .original_qty = qty,
    });
    processOrder(id, type, side, qty, price, flag);
}

template <class Notification>
void OrderBook<Notification>::processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag) {
    const auto tradeNotification = [this](OrderID mOrderID, OrderID tOrderID, OrderStatus mOrderStatus, OrderStatus tOrderStatus, Decimal qty, Decimal price) {
        this->putTradeNotification(mOrderID, tOrderID, mOrderStatus, tOrderStatus, qty, price);
        this->last_price = price;
    };
    const auto postOrderFill = [this](OrderID id) { this->eraseOrder(id); };

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

        orders_.insert(id, o);
    }

    return;
}

template <class Notification>
void OrderBook<Notification>::putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {
    notification_.onExecutionReport(ExecutionReport{
        .exec_type = ExecType::Trade,
        .maker_order_id = mOrderID,
        .taker_order_id = tOrderID,
        .maker_status = mStatus,
        .taker_status = tStatus,
        .last_qty = qty,
        .last_price = price,
    });
}

template <class Notification>
void OrderBook<Notification>::cancelOrder(OrderID id) {
    auto [qty, original_qty] = eraseOrder(id);
    if (qty.is_zero()) {
        notification_.onExecutionReport(ExecutionReport{
            .exec_type = ExecType::Rejected,
            .msg_type = MsgType::CancelOrder,
            .order_id = id,
            .status = OrderStatus::Rejected,
            .qty = uint64_t(0),
            .original_qty = uint64_t(0),
            .error = Error::OrderNotExists,
        });
        return;
    }

    notification_.onExecutionReport(ExecutionReport{
        .exec_type = ExecType::Canceled,
        .msg_type = MsgType::CancelOrder,
        .order_id = id,
        .status = OrderStatus::Canceled,
        .qty = qty,
        .original_qty = original_qty,
    });
}

template <class Notification>
std::pair<Decimal, Decimal> OrderBook<Notification>::eraseOrder(OrderID id) {
    auto* order = orders_.erase(id);
    if (order == nullptr) {
        return {uint64_t(0), uint64_t(0)};
    }

    auto& pool = order_pool_;
    scope_exit defer([&pool, &order]() { pool.release(order); });

    if (order->side == Side::Buy) {
        bids_.remove(order);
        return {order->qty, order->original_qty};
    }

    asks_.remove(order);
    return {order->qty, order->original_qty};
}

template <class Notification>
bool OrderBook<Notification>::hasOrder(OrderID id) {
    return orders_.contains(id);
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
