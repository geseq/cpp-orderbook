#include <map>

#include "boost/intrusive/rbtree.hpp"
#include "pricelevel.hpp"

namespace orderbook {

using OrderMap = boost::intrusive::rbtree<Order>;

class OrderBook {
   public:
    OrderBook(Notification n)
        : notification_(std::move(n)),
          bids_(PriceLevel<CmpGreater>(BidPrice)),
          asks_(PriceLevel<CmpLess>(AskPrice)),
          trigger_over_(PriceLevel<CmpGreater>(TrigPrice)),
          trigger_under_(PriceLevel<CmpLess>(TrigPrice)),
          orders_(OrderMap()),
          trig_orders_(OrderMap()){};

    void addOrder(uint64_t tok, uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void putTradeNotification(uint64_t mOrderId, uint64_t tOrderId, OrderStatus mStatus, OrderStatus tStatus, Decimal qty, Decimal price);
    std::shared_ptr<Order> cancelOrder(OrderId id);

    Decimal last_price = 0;

   private:
    PriceLevel<CmpGreater> bids_;
    PriceLevel<CmpLess> asks_;
    PriceLevel<CmpGreater> trigger_over_;
    PriceLevel<CmpLess> trigger_under_;

    OrderMap orders_;
    OrderMap trig_orders_;

    Notification notification_;

    std::atomic_uint64_t last_token_ = 0;

    bool matching_ = false;

    void addTrigOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Decimal trigPrice, Flag flag);
    void processOrder(uint64_t id, Type type, Side side, Decimal qty, Decimal price, Flag flag);
};

}  // namespace orderbook
