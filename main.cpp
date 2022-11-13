#include <iostream>
#include "src/orderbook.hpp"

int main() {
    auto on = OrderNotification();
    auto tn = TradeNotification();
    auto ob = OrderBook(on, tn);
    std::cout <<"test";
}
