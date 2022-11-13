#include <cstdint>
#include <memory>

#include "types.hpp"

class OrderQueue;

class Order {
    OrderId id_;
    Decimal qty_;
    Decimal price_;
    Decimal trig_price_;
    Type type_;
    Flag flag_;

    std::shared_ptr<Order> prev_ = nullptr;
    std::shared_ptr<Order> next_ = nullptr;
    std::shared_ptr<OrderQueue> queue_ = nullptr;

   public:
    Order(Decimal id, Decimal qty, Decimal price, Type type, Flag flag)
        : id_(id), qty_(qty), price_(price), type_(type), flag_(flag) {
        trig_price_ = 0;
    };

    Order(OrderId id, Decimal qty, Decimal price, Decimal trig_price, Type type,
          Flag flag)
        : id_(id),
          qty_(qty),
          price_(price),
          trig_price_(trig_price),
          type_(type),
          flag_(flag){};
};

