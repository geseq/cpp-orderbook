#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "pricelevel.hpp"

namespace orderbook {

class Order;

using OrderMap = std::map<OrderId, std::shared_ptr<Order>>;

class Notification {
   public:
    virtual void putOrder(MsgType msgtype, OrderStatus status, uint64_t id, Decimal qty, Error err) {}
    virtual void putOrder(MsgType msgtype, OrderStatus status, uint64_t id, Decimal qty) {}

    virtual void putTrade(uint64_t mOrderId, uint64_t tOrderId, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price) {}
};

class OrderBook {
   public:
    OrderBook(Notification n)
        : notification_(std::move(n)),
          bids_(PriceLevel(BidPrice)),
          asks_(PriceLevel(AskPrice)),
          trigger_over_(PriceLevel(TrigPrice)),
          trigger_under_(PriceLevel(TrigPrice)),
          orders_(OrderMap()),
          trig_orders_(OrderMap()){};

    auto addOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);

   private:
    PriceLevel bids_;
    PriceLevel asks_;
    PriceLevel trigger_over_;
    PriceLevel trigger_under_;

    OrderMap orders_;
    OrderMap trig_orders_;

    Notification notification_;

    Decimal last_price_ = 0;
    std::atomic_uint64_t last_token_ = 0;

    bool matching_ = false;

    void addTrigOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
};

auto OrderBook::addOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag) {
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
        if (trigPrice == 0) {
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
    // TODO
    return;
}
}  // namespace orderbook
