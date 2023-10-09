#include "orderbook.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "util_test.h"

using orderbook::OrderBook;

TEST(OrderBookTests, TestLimitOrder_Create) {
    Notification n;
    OrderBook ob(n);

    for (int i = 50; i < 100; i += 10) {
        processLine(ob, std::to_string(i) + "	L	B	2	" + std::to_string(i) + "	0	N");
        // Check your conditions here based on your notification system
    }

    for (int i = 100; i < 150; i += 10) {
        processLine(ob, std::to_string(i) + "	L	S	2	" + std::to_string(i) + "	0	N");
        // Check your conditions here based on your notification system
    }

    // Assuming Order is a pointer in OrderMap
    EXPECT_EQ(nullptr, ob.cancelOrder(999));
    EXPECT_NE(nullptr, ob.cancelOrder(100));
}

TEST(OrderBookTests, TestLimitOrder_CreateBuy) {
    MockNotification n;
    OrderBook ob(n);
    addDepth(ob);

    processLine(ob, "1100	L	B	1	100	0	N");
    // Verify conditions here based on your notification system

    processLine(ob, "1150	L	B	10	150	0	N");
    // Verify conditions here based on your notification system
}

