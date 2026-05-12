#pragma once

#include <cstdint>
#include <cwchar>
#include <iostream>
#include <optional>

#include "decimal.hpp"

namespace orderbook {

using Decimal = decimal::U8;
using OrderID = uint64_t;
using BookOrderID = uint64_t;
using SeqNum = uint64_t;

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
};

std::ostream& operator<<(std::ostream& os, const PriceType& priceType);

enum class Error : uint16_t {
    InvalidQty,
    InvalidPrice,
    InvalidSeqNum,
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
    Snapshot = 8,
};

std::ostream& operator<<(std::ostream& os, const Flag& flag);

enum class ExecType : uint8_t {
    New,
    Rejected,
    Canceled,
    Trade,
};

std::ostream& operator<<(std::ostream& os, const ExecType& execType);

struct ExecutionReport {
    ExecType exec_type{};

    // Order event fields (New, Rejected, Canceled)
    MsgType msg_type{};
    OrderID ref_order_id{};
    OrderStatus status{};
    Decimal qty{};
    Decimal original_qty{};

    // Trade event fields
    OrderID maker_ref_order_id{};
    OrderID taker_ref_order_id{};
    OrderStatus maker_status{};
    OrderStatus taker_status{};
    Decimal last_qty{};
    Decimal last_price{};

    // Rejection reason (Rejected only)
    std::optional<Error> error{};
};

// Notification is the interface for actual implementation of a notification handler
template <typename Implementation>
class NotificationInterface {
   public:
    void onExecutionReport(const ExecutionReport& report) {
        static_cast<Implementation*>(this)->onExecutionReport(report);
    }
};

class EmptyNotification : public NotificationInterface<EmptyNotification> {
   public:
    void onExecutionReport(const ExecutionReport&) {}
};

}  // namespace orderbook
