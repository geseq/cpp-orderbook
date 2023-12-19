#include <iostream>

#include "include/orderbook.hpp"
#include "include/types.hpp"

int main() {
    auto n = orderbook::EmptyNotification();
    auto ob = orderbook::OrderBook<orderbook::EmptyNotification>(n);
    std::cout << "test";
}
