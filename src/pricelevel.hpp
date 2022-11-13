#include <cassert>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <map>

#include "orderqueue.hpp"

class PriceLevel {
    std::map<Decimal, OrderQueue> price_tree_;

    PriceType price_type_;
    Decimal volume_ = 0;
    uint64_t num_orders_ = 0;
    int depth_ = 0;

   public:
    PriceLevel(PriceType price_type) : price_type_(price_type) {}
};
