#include <map>

#include "pricelevel.hpp"

namespace orderbook {

using OrderMap = std::map<OrderId, std::shared_ptr<Order>>;

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

    void addOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void putTradeNotification(uint64_t mOrderId, uint64_t tOrderId, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    std::shared_ptr<Order> cancelOrder(OrderId id);

    Decimal last_price = 0;

   private:
    PriceLevel bids_;
    PriceLevel asks_;
    PriceLevel trigger_over_;
    PriceLevel trigger_under_;

    OrderMap orders_;
    OrderMap trig_orders_;

    Notification notification_;

    std::atomic_uint64_t last_token_ = 0;

    bool matching_ = false;

    void addTrigOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
};

}  // namespace orderbook
