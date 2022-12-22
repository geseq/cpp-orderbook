#include <cstdint>

namespace orderbook {

using Flag = uint8_t;
using Decimal = uint64_t;
using OrderId = uint64_t;

enum Type : uint8_t {
    Limit,
    Market,
};

enum Side : uint8_t { Buy, Sell };

enum MsgType : uint8_t { MsgCreateOrder, MsgCancelOrder };

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

}  // namespace orderbook
