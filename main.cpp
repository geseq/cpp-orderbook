#include <iostream>

#include "src/orderbook.hpp"

int main() {
    auto n = orderbook::Notification();
    auto ob = orderbook::OrderBook(n);
    std::cout << "test";
}
