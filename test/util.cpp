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

class Notification : public orderbook::NotificationInterface<Notification> {
   public:
    void Reset() { reports_.clear(); }

    void onExecutionReport(const orderbook::ExecutionReport& r) { reports_.emplace_back(r); }

    std::vector<std::string> Strings() const {
        std::vector<std::string> res;
        for (const auto& r : reports_) {
            res.push_back(to_string(r));
        }
        return res;
    }

    std::string String() const {
        std::ostringstream oss;
        for (const auto& r : reports_) {
            oss << to_string(r) << '\n';
        }
        return oss.str();
    }

    bool hasError() {
        for (const auto& r : reports_) {
            if (r.exec_type == orderbook::ExecType::Rejected) {
                return true;
            }
        }
        return false;
    }

    void Verify(const std::vector<std::string>& expected) const { ASSERT_EQ(expected, Strings()); }

   private:
    std::vector<orderbook::ExecutionReport> reports_;

    static std::string to_string(const orderbook::ExecutionReport& r) {
        std::ostringstream os;
        if (r.exec_type == orderbook::ExecType::Trade) {
            os << r.maker_ref_order_id << " " << r.taker_ref_order_id << " " << r.maker_status << " " << r.taker_status << " " << r.last_qty.to_string() << " "
               << r.last_price.to_string();
        } else {
            os << r.msg_type << " " << r.status << " " << r.ref_order_id << " " << r.qty << " " << r.original_qty;
            if (r.error.has_value()) {
                os << " Err" << *r.error;
            }
        }
        return os.str();
    }
};

