#pragma once

#include <map>

#include "boost/intrusive/rbtree.hpp"
#include "pricelevel.hpp"
#include "types.hpp"

namespace orderbook {

using OrderMap = boost::intrusive::rbtree<Order, CmpLess>;

template <class Notification>
class OrderBook {
   public:
    OrderBook(NotificationInterface<Notification>& n, size_t price_level_pool_size = 16384, size_t order_pool_size = 16384)
        : order_pool_(order_pool_size),
          notification_(n),
          bids_(PriceLevel<CmpGreater>(PriceType::Bid, price_level_pool_size)),
          asks_(PriceLevel<CmpLess>(PriceType::Ask, price_level_pool_size)),
          trigger_over_(PriceLevel<CmpGreater>(PriceType::Trigger, price_level_pool_size)),
          trigger_under_(PriceLevel<CmpLess>(PriceType::Trigger, price_level_pool_size)),
          orders_(OrderMap()),
          trig_orders_(OrderMap()){};

    void addOrder(uint64_t tok, OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void putTradeNotification(OrderID mOrderID, OrderID tOrderID, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    void cancelOrder(uint64_t tok, OrderID id);
    bool hasOrder(OrderID id);

    std::string toString();

    Decimal last_price;

   private:
    pool::AdaptiveObjectPool<Order> order_pool_;

    PriceLevel<CmpGreater> bids_;
    PriceLevel<CmpLess> asks_;
    PriceLevel<CmpGreater> trigger_over_;
    PriceLevel<CmpLess> trigger_under_;

    OrderMap orders_;
    OrderMap trig_orders_;

    NotificationInterface<Notification>& notification_;

    std::atomic_uint64_t last_token_ = 0;

    std::atomic_uint64_t matching_ = 1;

    std::shared_ptr<Order> cancelOrder(OrderID id);
    void addTrigOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(OrderID id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
    void postProcess(Decimal& lp);
    void queueTriggeredOrders();
    void processTriggeredOrders();
};

}  // namespace orderbook
