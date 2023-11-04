#pragma once

#include <map>

#include "boost/intrusive/rbtree.hpp"
#include "pricelevel.hpp"

namespace orderbook {

using OrderMap = boost::intrusive::rbtree<Order, CmpLess>;

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

    void addOrder(uint64_t tok, OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    void cancelOrder(uint64_t tok, OrderID id);
    bool hasOrder(OrderID id);
    friend Decimal OrderQueue::process(OrderBook& ob, OrderID takerOrderID, Decimal qty);

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

    std::shared_ptr<Order> cancelOrder(OrderID id);
    void addTrigOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
    void postProcess(Decimal& lp);
    void queueTriggeredOrders();
    void processTriggeredOrders();
};

}  // namespace orderbook
