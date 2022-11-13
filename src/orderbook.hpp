#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "pricelevel.hpp"

class Order;

using OrderMap = std::unordered_map<OrderId, std::shared_ptr<Order>>;

class OrderNotification {
   public:
    virtual void PutOrder(MsgType msgtype, OrderStatus status, uint64_t id,
                          Decimal qty, Error err) {}
};

class TradeNotification {
   public:
    virtual void PutOrder(MsgType msgtype, uint64_t id, Decimal qty,
                          Error err) {}
};

class OrderBook {
   public:
    OrderBook(OrderNotification on, TradeNotification tn)
        : orderNotification_(std::move(on)), tradeNotification_(std::move(tn)) {
        bids_ = std::make_unique<PriceLevel>(BidPrice);
        asks_ = std::make_unique<PriceLevel>(AskPrice);
        trigger_over_ = std::make_unique<PriceLevel>(TrigPrice);
        trigger_under_ = std::make_unique<PriceLevel>(TrigPrice);

        orders_ = std::make_unique<OrderMap>();
        trig_orders_ = std::make_unique<OrderMap>();
    };

    auto AddOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty,
                  Decimal price, Decimal trigPrice, Flag flag);

   private:
    std::unique_ptr<PriceLevel> bids_;
    std::unique_ptr<PriceLevel> asks_;
    std::unique_ptr<PriceLevel> trigger_over_;
    std::unique_ptr<PriceLevel> trigger_under_;

    std::unique_ptr<OrderMap> orders_;
    std::unique_ptr<OrderMap> trig_orders_;

    OrderNotification orderNotification_;
    TradeNotification tradeNotification_;

    Decimal last_price_ = 0;
    std::atomic_uint64_t last_token_ = 0;

    bool matching_ = false;
};

auto OrderBook::AddOrder(uint64_t tok, uint64_t id, Type type, Side side,
                         Decimal qty, Decimal price, Decimal trigPrice,
                         Flag flag) {
    uint64_t exp = tok - 1;
    if (!last_token_.compare_exchange_strong(exp, tok)) {
        throw std::invalid_argument(
            "invalid token received: cannot maintain determinism");
    }

    if (!matching_) {
        if (type == Market) {
        }
    }
}

