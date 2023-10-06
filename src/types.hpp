#include <cstdint>

#include "udecimal.hpp"

namespace orderbook {

using Decimal = udecimal::Decimal<8>;
using OrderId = uint64_t;

enum Type : uint8_t {
    Limit,
    Market,
};

enum Side : uint8_t {
    Buy,
    Sell
};

enum MsgType : uint8_t {
    MsgCreateOrder,
    MsgCancelOrder
};

enum OrderStatus : uint8_t {
    OrderRejected,
    OrderCanceled,
    OrderFilledParial,
    OrderFilledComplete,
    OrderAccepted,
};

enum PriceType {
    BidPrice,
    AskPrice,
    TrigPrice,
};

enum Error : uint16_t {
    ErrInvalidQty,
    ErrInvalidPrice,
    ErrInvalidTriggerPrice,
    ErrOrderID,
    ErrOrderExists,
    ErrOrderNotExists,
    ErrInsufficientQty,
    ErrNoMatching,
};

enum Flag : uint8_t {
    None = 0,
    IoC = 1,
    AoN = 2,
    FoK = 4,
    StopLoss = 8,
    TakeProfit = 16,
    Snapshot = 32
};

class Notification {
   public:
    virtual void putOrder(MsgType msgtype, OrderStatus status, uint64_t id, Decimal qty, Error err) {}
    virtual void putOrder(MsgType msgtype, OrderStatus status, uint64_t id, Decimal qty) {}

    virtual void putTrade(uint64_t mOrderId, uint64_t tOrderId, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {}
};

}  // namespace orderbook
