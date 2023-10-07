#include <iostream>

#include "include/orderbook.hpp"

int main() {
    auto n = orderbook::Notification();
    auto ob = orderbook::OrderBook(n);
    std::cout << "test";
}
