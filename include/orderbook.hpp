#pragma once

#include <map>

#include "boost/intrusive/rbtree.hpp"
#include "pricelevel.hpp"

namespace orderbook {

using OrderMap = boost::intrusive::rbtree<Order, boost::intrusive::compare<OrderIDCompare>>;

class OrderBook {
   public:
    OrderBook(Notification& n)
        : notification_(n),
          bids_(PriceLevel<CmpGreater>(PriceType::Bid)),
          asks_(PriceLevel<CmpLess>(PriceType::Ask)),
          trigger_over_(PriceLevel<CmpGreater>(PriceType::Trigger)),
          trigger_under_(PriceLevel<CmpLess>(PriceType::Trigger)),
          orders_(OrderMap()),
          trig_orders_(OrderMap()){};

    void addOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void putTradeNotification(uint64_t mOrderID, uint64_t tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    void cancelOrder(uint64_t tok, OrderID id);
    std::shared_ptr<Order> cancelOrder(OrderID id);
    bool hasOrder(OrderID id);

    std::string toString();

    Decimal last_price;

   private:
    PriceLevel<CmpGreater> bids_;
    PriceLevel<CmpLess> asks_;
    PriceLevel<CmpGreater> trigger_over_;
    PriceLevel<CmpLess> trigger_under_;

    OrderMap orders_;
    OrderMap trig_orders_;

    Notification& notification_;

    std::atomic_uint64_t last_token_ = 0;

    bool matching_ = true;

    void addTrigOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
};

}  // namespace orderbook
