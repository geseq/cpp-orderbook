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
    FilledParial,
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

class Notification {
   public:
    virtual void putOrder(MsgType msgtype, OrderStatus status, OrderID id, Decimal qty, Error err) {}
    virtual void putOrder(MsgType msgtype, OrderStatus status, OrderID id, Decimal qty) {}

    virtual void putTrade(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {}
};

}  // namespace orderbook
