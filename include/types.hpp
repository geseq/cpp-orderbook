#pragma once

#include <cstdint>
#include <iostream>

#include "udecimal.hpp"

namespace orderbook {

using Decimal = udecimal::Decimal<8>;
using OrderID = uint64_t;

enum class Type : uint8_t {
    Limit,
    Market,
};

std::ostream& operator<<(std::ostream& os, const Type& type);

enum class Side : uint8_t {
    Buy,
    Sell
};

std::ostream& operator<<(std::ostream& os, const Side& side);

enum class MsgType : uint8_t {
    CreateOrder,
    CancelOrder
};

std::ostream& operator<<(std::ostream& os, const MsgType& msgType);

enum class OrderStatus : uint8_t {
    Rejected,
    Canceled,
    FilledPartial,
    FilledComplete,
    Accepted,
};

std::ostream& operator<<(std::ostream& os, const OrderStatus& status);

enum class PriceType {
    Bid,
    Ask,
    Trigger,
};

std::ostream& operator<<(std::ostream& os, const PriceType& priceType);

enum class Error : uint16_t {
    InvalidQty,
    InvalidPrice,
    InvalidTriggerPrice,
    OrderID,
    OrderExists,
    OrderNotExists,
    InsufficientQty,
    NoMatching,
};

std::ostream& operator<<(std::ostream& os, const Error& error);

enum Flag : uint8_t {
    None = 0,
    IoC = 1,
    AoN = 2,
    FoK = 4,
    StopLoss = 8,
    TakeProfit = 16,
    Snapshot = 32
};

std::ostream& operator<<(std::ostream& os, const Flag& flag);

struct Trade {
    uint64_t MakerOrderID;
    uint64_t TakerOrderID;
    OrderStatus MakerStatus;
    OrderStatus TakerStatus;
    Decimal Qty;
    Decimal Price;
};

// Notification is the interface for actual implementation of a notification handler
template <typename Implementation>
class NotificationInterface {
   public:
    void putOrder(MsgType msgtype, OrderStatus status, OrderID id, Decimal qty, Error err) {
        static_cast<Implementation*>(this)->putOrder(msgtype, status, id, qty, err);
    }

    void putOrder(MsgType msgtype, OrderStatus status, OrderID id, Decimal qty) { static_cast<Implementation*>(this)->putOrder(msgtype, status, id, qty); }

    void putTrade(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {
        static_cast<Implementation*>(this)->putTrade(mOrderID, tOrderID, mStatus, tStatus, qty, price);
    }
};

class EmptyNotification : public NotificationInterface<EmptyNotification> {
   public:
    void putOrder(MsgType msgtype, OrderStatus status, OrderID id, Decimal qty, Error err) {}

    void putOrder(MsgType msgtype, OrderStatus status, OrderID id, Decimal qty) {}

    void putTrade(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {}
};

}  // namespace orderbook
