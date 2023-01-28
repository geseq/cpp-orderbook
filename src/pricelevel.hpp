#include <map>

#include "orderqueue.hpp"

namespace orderbook {

class OrderBook;

class Compare {
   public:
    bool LessThanOrEqual(Decimal price);
    bool GreaterThanOrEqual(Decimal price);
};

class PriceLevel {
    std::map<Decimal, std::shared_ptr<OrderQueue>> price_tree_;

    PriceType price_type_;
    Decimal volume_ = 0;
    uint64_t num_orders_ = 0;
    uint64_t depth_ = 0;

    std::shared_ptr<OrderQueue> getMinPriceQueue();
    std::shared_ptr<OrderQueue> getMaxPriceQueue();

   public:
    PriceLevel(PriceType price_type) : price_type_(price_type){};
    uint64_t len();
    uint64_t depth();
    Decimal volume();
    std::shared_ptr<OrderQueue> getQueue();
    void append(const std::shared_ptr<Order>& order);
    void remove(const std::shared_ptr<Order>& order);
    Decimal processMarketOrder(OrderBook* ob, OrderId takerOrderId, Decimal qty, Flag flag);
    Decimal processLimitOrder(OrderBook* ob, bool(Decimal), OrderId takerOrderId, Decimal qty, Flag flag);
};

}  // namespace orderbook

