#include "types.hpp"

namespace orderbook {

std::ostream& operator<<(std::ostream& os, const Type& type) {
    switch (type) {
        case Type::Limit:
            return os << "Limit";
        case Type::Market:
            return os << "Market";
    }
    return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Side& side) {
    switch (side) {
        case Side::Buy:
            return os << "Buy";
        case Side::Sell:
            return os << "Sell";
    }
    return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const MsgType& msgType) {
    switch (msgType) {
        case MsgType::CreateOrder:
            return os << "CreateOrder";
        case MsgType::CancelOrder:
            return os << "CancelOrder";
    }
    return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const OrderStatus& status) {
    switch (status) {
        case OrderStatus::Rejected:
            return os << "Rejected";
        case OrderStatus::Canceled:
            return os << "Canceled";
        case OrderStatus::FilledPartial:
            return os << "FilledPartial";
        case OrderStatus::FilledComplete:
            return os << "FilledComplete";
        case OrderStatus::Accepted:
            return os << "Accepted";
    }
    return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const PriceType& priceType) {
    switch (priceType) {
        case PriceType::Bid:
            return os << "Bid";
        case PriceType::Ask:
            return os << "Ask";
        case PriceType::TriggerOver:
            return os << "TriggerOver";
        case PriceType::TriggerUnder:
            return os << "TriggerUnder";
    }
    return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Error& error) {
    switch (error) {
        case Error::InvalidQty:
            return os << "InvalidQty";
        case Error::InvalidPrice:
            return os << "InvalidPrice";
        case Error::InvalidTriggerPrice:
            return os << "InvalidTriggerPrice";
        case Error::OrderID:
            return os << "OrderID";
        case Error::OrderExists:
            return os << "OrderExists";
        case Error::OrderNotExists:
            return os << "OrderNotExists";
        case Error::InsufficientQty:
            return os << "InsufficientQty";
        case Error::NoMatching:
            return os << "NoMatching";
    }
    return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Flag& flag) {
    switch (flag) {
        case Flag::None:
            return os << "None";
        case Flag::IoC:
            return os << "IoC";
        case Flag::AoN:
            return os << "AoN";
        case Flag::FoK:
            return os << "FoK";
        case Flag::StopLoss:
            return os << "StopLoss";
        case Flag::TakeProfit:
            return os << "TakeProfit";
        case Flag::Snapshot:
            return os << "Snapshot";
    }
    return os << "Unknown";
}

}  // namespace orderbook
