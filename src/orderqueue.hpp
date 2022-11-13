#include <cstdint>
#include <memory>

#include "types.hpp"

class Order;

class OrderQueue {
   public:
    OrderQueue(Decimal price) : price_(price){};

   private:
    std::shared_ptr<Order> head_ = nullptr;
    std::shared_ptr<Order> tail_ = nullptr;

    Decimal price_ = 0;
    Decimal total_qty_ = 0;
    uint64_t size_ = 0;
};

