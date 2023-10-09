#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "orderbook.hpp"

using orderbook::Decimal;
using orderbook::Flag;
using orderbook::MsgType;
using orderbook::OrderBook;
using orderbook::OrderID;
using orderbook::OrderStatus;
using orderbook::Side;
using orderbook::Type;

class NotificationBase {
   public:
    virtual ~NotificationBase() {}
    virtual std::string to_string() const = 0;
};

struct OrderNotification : public NotificationBase {
    orderbook::MsgType msg_type_;
    orderbook::OrderStatus status_;
    orderbook::OrderID order_id_;
    orderbook::Decimal qty_;
    std::optional<orderbook::Error> error_;

    OrderNotification(orderbook::MsgType msg_type, orderbook::OrderStatus status, orderbook::OrderID order_id, orderbook::Decimal qty,
                      std::optional<orderbook::Error> error)
        : msg_type_(msg_type), status_(status), order_id_(order_id), qty_(qty), error_(error){};

    std::string to_string() const override {
        std::ostringstream os;
        if (error_.has_value()) {
            os << msg_type_ << " " << status_ << " " << order_id_ << " " << qty_.to_string() << " Err" << *error_;
        } else {
            os << msg_type_ << " " << status_ << " " << order_id_ << " " << qty_.to_string();
        }
        return os.str();
    }
};

struct Trade : public NotificationBase {
    OrderID MakerOrderID;
    OrderID TakerOrderID;
    OrderStatus MakerStatus;
    OrderStatus TakerStatus;
    Decimal Qty;
    Decimal Price;

    Trade(OrderID mID, OrderID tID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price)
        : MakerOrderID(mID), TakerOrderID(tID), MakerStatus(mStatus), TakerStatus(tStatus), Qty(qty), Price(price) {}

    std::string to_string() const override {
        std::ostringstream os;
        os << MakerOrderID << " " << TakerOrderID << " " << MakerStatus << " " << TakerStatus << " " << Qty.to_string() << " " << Price.to_string();
        return os.str();
    }
};

class Notification : public orderbook::Notification {
   public:
    void Reset() { n.clear(); }

    void putOrder(MsgType m, OrderStatus s, OrderID orderID, Decimal qty, orderbook::Error err) override {
        auto notification = std::make_shared<OrderNotification>(m, s, orderID, qty, err);
        n.emplace_back(notification);
    }

    void putOrder(MsgType m, OrderStatus s, OrderID orderID, Decimal qty) override {
        auto notification = std::make_shared<OrderNotification>(m, s, orderID, qty, std::optional<orderbook::Error>{});
        n.emplace_back(notification);
    }

    void putTrade(OrderID mID, OrderID tID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) override {
        auto notification = std::make_shared<Trade>(mID, tID, mStatus, tStatus, qty, price);
        n.emplace_back(notification);
    }

    std::vector<std::string> Strings() const {
        std::vector<std::string> res;
        for (const auto& item : n) {
            res.push_back(item->to_string());
        }
        return res;
    }

    std::string String() const {
        std::ostringstream oss;
        for (const auto& item : n) {
            oss << item->to_string() << '\n';
        }
        return oss.str();
    }

    bool hasError() {
        for (const auto& item : n) {
            auto notification = dynamic_cast<OrderNotification*>(item.get());
            if (notification != nullptr && notification->error_) {
                return true;
            }
        }

        return false;
    }

    void Verify(const std::vector<std::string>& expected) const { ASSERT_EQ(expected, Strings()); }

   private:
    std::vector<std::shared_ptr<NotificationBase>> n;
};

