#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "util.cpp"

class LimitOrderTest : public ::testing::Test {
   protected:
    Notification n;
    std::shared_ptr<OrderBook<Notification>> ob;

    void SetUp() override {
        n = Notification();
        ob = std::make_shared<orderbook::OrderBook<Notification>>(n);
    }

    void TearDown() override {}

    void processLine(std::shared_ptr<OrderBook<Notification>>& ob, const std::string& line) {
        std::vector<std::string> parts;
        boost::split(parts, line, boost::is_any_of("\t"));
        if (parts.empty()) {
            return;
        }

        uint64_t oid = std::stoull(parts[0]);
        Type type = (parts[1] == "L") ? orderbook::Type::Limit : orderbook::Type::Market;
        Side side = (parts[2] == "S") ? orderbook::Side::Sell : orderbook::Side::Buy;
        Decimal qty = parts[3];
        Decimal price = parts[4];
        Flag flag = orderbook::Flag::None;
        if (parts[5] == "A") {
            flag = orderbook::AoN;
        } else if (parts[5] == "I") {
            flag = orderbook::IoC;
        } else if (parts[5] == "F") {
            flag = orderbook::FoK;
        }

        ob->addOrder(oid, type, side, qty, price, flag);
    }

    void processOrders(std::shared_ptr<OrderBook<Notification>>& ob, const std::string& input, int prefix) {
        std::stringstream ss(input);
        std::string line;

        while (std::getline(ss, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            if (prefix > 0) {
                line = std::to_string(prefix) + line;
            }
            processLine(ob, line);
        }
    }

    void addDepth(std::shared_ptr<OrderBook<Notification>>& ob, int prefix = 0) {
        const static std::string depth = R"(
# add depth to the orderbook
1	L	B	2	50	N
2	L	B	2	60	N
3	L	B	2	70	N
4	L	B	2	80	N
5	L	B	2	90	N
6	L	S	2	100	N
7	L	S	2	110	N
8	L	S	2	120	N
9	L	S	2	130	N
10	L	S	2	140	N
)";

        processOrders(ob, depth, prefix);
    }
};

TEST_F(LimitOrderTest, TestFlagsAreBitmaskCompatibleForAoNAndFoKChecks) {
    EXPECT_EQ(orderbook::IoC, 1);
    EXPECT_EQ(orderbook::AoN, 2);
    EXPECT_EQ(orderbook::FoK, 4);
    EXPECT_EQ(orderbook::Snapshot, 8);

    EXPECT_NE((orderbook::AoN | orderbook::FoK) & orderbook::AoN, 0);
    EXPECT_NE((orderbook::AoN | orderbook::FoK) & orderbook::FoK, 0);
    EXPECT_EQ((orderbook::AoN | orderbook::FoK) & orderbook::IoC, 0);
}

TEST_F(LimitOrderTest, TestLimitOrder_Create) {
    for (int i = 50; i < 100; i += 10) {
        n.Reset();
        processLine(ob, std::to_string(i) + "	L	B	2	" + std::to_string(i) + "	N");
        n.Verify({"CreateOrder Accepted " + std::to_string(i) + " 2 2"});
    }

    for (int i = 100; i < 150; i += 10) {
        n.Reset();
        processLine(ob, std::to_string(i) + "	L	S	2	" + std::to_string(i) + "	N");
        n.Verify({"CreateOrder Accepted " + std::to_string(i) + " 2 2"});
    }

    ASSERT_FALSE(ob->hasOrder(999));
    ASSERT_TRUE(ob->hasOrder(100));
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateBuy) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "1100	L	B	1	100	N");
    n.Verify({"CreateOrder Accepted 1100 1 1", "6 1100 FilledPartial FilledComplete 1 100"});

    n.Reset();
    processLine(ob, "1150	L	B	10	150	N");
    // clang-format off
    n.Verify({"CreateOrder Accepted 1150 10 10", 
          "6 1150 FilledComplete FilledPartial 1 100", 
          "7 1150 FilledComplete FilledPartial 2 110",
          "8 1150 FilledComplete FilledPartial 2 120", 
          "9 1150 FilledComplete FilledPartial 2 130", 
          "10 1150 FilledComplete FilledPartial 2 140"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateWithZeroQty) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "170	L	S	0	40	N");
    n.Verify({"CreateOrder Rejected 170 0 0 ErrInvalidQty"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateWithZeroPrice) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "170	L	S	10	0	N");
    n.Verify({"CreateOrder Rejected 170 0 10 ErrInvalidPrice"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateDuplicateOrderID) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "170	L	S	10	1000	N");
    processLine(ob, "170	L	S	5	1000	N");
    n.Verify({"CreateOrder Accepted 170 10 10", "CreateOrder Rejected 170 0 5 ErrOrderExists"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateAndCancel) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "170	L	S	10	1000	N");
    ob->cancelOrder(170);
    // clang-format off
    n.Verify({"CreateOrder Accepted 170 10 10", 
            "CancelOrder Canceled 170 10 10"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_CancelNonExistent) {
    addDepth(ob);

    n.Reset();
    ob->cancelOrder(170);
    n.Verify({"CancelOrder Rejected 170 0 0 ErrOrderNotExists"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateIOCWithNoMatches) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "300	L	S	1	200	I");
    n.Verify({"CreateOrder Accepted 300 1 1"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateIoCWithMatches) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "300	L	S	1	90	I");
    // clang-format off
    n.Verify({"CreateOrder Accepted 300 1 1", 
            "5 300 FilledPartial FilledComplete 1 90"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateSell) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "340	L	S	11	40	N");
    processLine(ob, "343	L	S	11	1	I");

    // clang-format off
    n.Verify({"CreateOrder Accepted 340 11 11", 
              "5 340 FilledComplete FilledPartial 2 90", 
              "4 340 FilledComplete FilledPartial 2 80", 
              "3 340 FilledComplete FilledPartial 2 70", 
              "2 340 FilledComplete FilledPartial 2 60", 
              "1 340 FilledComplete FilledPartial 2 50", 
              "CreateOrder Accepted 343 11 11"});

    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_ClearSellBestPriceFirst) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "900	L	B	11	1	N");
    processLine(ob, "901	L	S	11	1	N");

    ASSERT_FALSE(n.hasError());
    // clang-format off
    n.Verify({"CreateOrder Accepted 900 11 11", 
              "CreateOrder Accepted 901 11 11",
              "5 901 FilledComplete FilledPartial 2 90",
              "4 901 FilledComplete FilledPartial 2 80",
              "3 901 FilledComplete FilledPartial 2 70",
              "2 901 FilledComplete FilledPartial 2 60",
              "1 901 FilledComplete FilledPartial 2 50",
              "900 901 FilledPartial FilledComplete 1 1"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestMarketProcess) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "800	M	B	3	0	N");
    processLine(ob, "801	M	B	0	0	N");
    processLine(ob, "802	M	S	12	0	N");
    processLine(ob, "803	M	B	12	0	A");
    processLine(ob, "804	M	B	12	0	N");

    // clang-format off
    n.Verify({"CreateOrder Accepted 800 3 3",
              "6 800 FilledComplete FilledPartial 2 100",
              "7 800 FilledPartial FilledComplete 1 110",
              "CreateOrder Rejected 801 0 0 ErrInvalidQty",
              "CreateOrder Accepted 802 12 12",
              "5 802 FilledComplete FilledPartial 2 90",
              "4 802 FilledComplete FilledPartial 2 80",
              "3 802 FilledComplete FilledPartial 2 70",
              "2 802 FilledComplete FilledPartial 2 60",
              "1 802 FilledComplete FilledPartial 2 50",
              "CreateOrder Accepted 803 12 12",
              "CreateOrder Accepted 804 12 12",
              "7 804 FilledComplete FilledPartial 1 110",
              "8 804 FilledComplete FilledPartial 2 120",
              "9 804 FilledComplete FilledPartial 2 130",
              "10 804 FilledComplete FilledPartial 2 140"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestMarketAoN_UsesCachedSideQtyAfterPartialFill) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "850	M	B	1	0	N");
    n.Verify({"CreateOrder Accepted 850 1 1", "6 850 FilledPartial FilledComplete 1 100"});

    n.Reset();
    processLine(ob, "851	M	B	10	0	A");
    n.Verify({"CreateOrder Accepted 851 10 10"});
}

TEST_F(LimitOrderTest, TestMarketProcess_PriceLevel_FIFO) {
    addDepth(ob, 0);
    addDepth(ob, 1);
    n.Reset();

    processLine(ob, "801	M	B	6	0	N");

    // clang-format off
        n.Verify({"CreateOrder Accepted 801 6 6",
                  "6 801 FilledComplete FilledPartial 2 100",
                  "16 801 FilledComplete FilledPartial 2 100",
                  "7 801 FilledComplete FilledComplete 2 110"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestNoMatchingMode_MarketOrderRejected) {
    addDepth(ob);
    ob->setMatching(false);
    n.Reset();

    // Market orders are always rejected in no-matching mode
    processLine(ob, "900	M	B	3	0	N");
    n.Verify({"CreateOrder Rejected 900 3 3 ErrNoMatching"});
}

TEST_F(LimitOrderTest, TestNoMatchingMode_LimitOrderCrossesSpread) {
    addDepth(ob);
    ob->setMatching(false);
    n.Reset();

    // Buy limit that would cross the best ask (100) → rejected
    processLine(ob, "901	L	B	2	100	N");
    n.Verify({"CreateOrder Rejected 901 2 2 ErrNoMatching"});
}

TEST_F(LimitOrderTest, TestNoMatchingMode_LimitSellOrderCrossesSpread) {
    addDepth(ob);
    ob->setMatching(false);
    n.Reset();

    // Sell limit that would cross the best bid (90) → rejected
    processLine(ob, "911	L	S	2	90	N");
    n.Verify({"CreateOrder Rejected 911 2 2 ErrNoMatching"});
}

TEST_F(LimitOrderTest, TestNoMatchingMode_LimitOrderNoMatch) {
    addDepth(ob);
    ob->setMatching(false);
    n.Reset();

    // Buy limit well below the best ask → accepted and resting
    processLine(ob, "902	L	B	2	50	N");
    n.Verify({"CreateOrder Accepted 902 2 2"});
}

TEST_F(LimitOrderTest, TestNoMatchingMode_LimitSellOrderNoMatch) {
    addDepth(ob);
    ob->setMatching(false);
    n.Reset();

    // Sell limit well above the best bid → accepted and resting
    processLine(ob, "912	L	S	2	200	N");
    n.Verify({"CreateOrder Accepted 912 2 2"});
}

// ──────────────────────────────────────────────────────────────────────────────
// IoC – Immediate or Cancel
// ──────────────────────────────────────────────────────────────────────────────

// Limit sell IoC: best bid exactly matches the limit price → one trade, rest cancelled (no book entry).
TEST_F(LimitOrderTest, TestIoC_LimitSell_ExactMatch) {
    addDepth(ob);
    n.Reset();

    // Sell IoC at 90 for qty 1 – best bid is 90 (qty 2), so a partial maker fill.
    processLine(ob, "300	L	S	1	90	I");
    // clang-format off
    n.Verify({"CreateOrder Accepted 300 1 1",
              "5 300 FilledPartial FilledComplete 1 90"});
    // clang-format on

    // The IoC order must NOT be resting in the book.
    ASSERT_FALSE(ob->hasOrder(300));
}

// Limit buy IoC: best ask exactly matches the limit price → trade, no book entry.
TEST_F(LimitOrderTest, TestIoC_LimitBuy_ExactMatch) {
    addDepth(ob);
    n.Reset();

    // Buy IoC at 100 for qty 1 – best ask is 100 (qty 2).
    processLine(ob, "400	L	B	1	100	I");
    // clang-format off
    n.Verify({"CreateOrder Accepted 400 1 1",
              "6 400 FilledPartial FilledComplete 1 100"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(400));
}

// Limit sell IoC: spans multiple bid price levels, unfilled qty is cancelled immediately.
TEST_F(LimitOrderTest, TestIoC_LimitSell_MultiLevel_PartialFill) {
    addDepth(ob);
    n.Reset();

    // Sell IoC at 70, qty 11 – bids at 90(2), 80(2), 70(2) = 6 available at or above 70.
    // Three levels fill; 5 units remain but are cancelled (IoC).
    processLine(ob, "301	L	S	11	70	I");
    // clang-format off
    n.Verify({"CreateOrder Accepted 301 11 11",
              "5 301 FilledComplete FilledPartial 2 90",
              "4 301 FilledComplete FilledPartial 2 80",
              "3 301 FilledComplete FilledPartial 2 70"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(301));
}

// Limit sell IoC: no matching bid price – no trades, order is silently cancelled.
TEST_F(LimitOrderTest, TestIoC_LimitSell_NoMatch) {
    addDepth(ob);
    n.Reset();

    // Sell IoC at 200 – all bids are below 200.
    processLine(ob, "302	L	S	1	200	I");
    n.Verify({"CreateOrder Accepted 302 1 1"});
    ASSERT_FALSE(ob->hasOrder(302));
}

// Limit buy IoC: no matching ask – no trades, order silently cancelled.
TEST_F(LimitOrderTest, TestIoC_LimitBuy_NoMatch) {
    addDepth(ob);
    n.Reset();

    // Buy IoC at 50 – best ask is 100, which is above 50 → no match.
    processLine(ob, "403	L	B	1	50	I");
    n.Verify({"CreateOrder Accepted 403 1 1"});
    ASSERT_FALSE(ob->hasOrder(403));
}

// Limit buy IoC: spans multiple ask levels, remaining qty cancelled.
TEST_F(LimitOrderTest, TestIoC_LimitBuy_MultiLevel_PartialFill) {
    addDepth(ob);
    n.Reset();

    // Buy IoC at 120, qty 11 – asks at 100(2), 110(2), 120(2) = 6 fillable.
    processLine(ob, "404	L	B	11	120	I");
    // clang-format off
    n.Verify({"CreateOrder Accepted 404 11 11",
              "6 404 FilledComplete FilledPartial 2 100",
              "7 404 FilledComplete FilledPartial 2 110",
              "8 404 FilledComplete FilledPartial 2 120"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(404));
}

// ──────────────────────────────────────────────────────────────────────────────
// AoN – All or Nothing (limit orders)
// ──────────────────────────────────────────────────────────────────────────────

// Limit sell AoN: enough liquidity at the limit price → fully executes.
TEST_F(LimitOrderTest, TestAoN_LimitSell_CanFill) {
    addDepth(ob);
    n.Reset();

    // Sell AoN at 90 for qty 2 – bid at 90 has exactly qty 2.
    processLine(ob, "500	L	S	2	90	A");
    // clang-format off
    n.Verify({"CreateOrder Accepted 500 2 2",
              "5 500 FilledComplete FilledComplete 2 90"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(500));
}

// Limit sell AoN: insufficient liquidity at or above the limit → no trades, order rests.
TEST_F(LimitOrderTest, TestAoN_LimitSell_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Sell AoN at 90 for qty 3 – bid at 90 only has qty 2, no other bid >= 90.
    processLine(ob, "501	L	S	3	90	A");
    n.Verify({"CreateOrder Accepted 501 3 3"});
    // AoN that can't fill immediately rests in the book.
    ASSERT_TRUE(ob->hasOrder(501));
}

// Limit buy AoN: enough liquidity at the limit price → fully executes.
TEST_F(LimitOrderTest, TestAoN_LimitBuy_CanFill) {
    addDepth(ob);
    n.Reset();

    // Buy AoN at 100 for qty 2 – ask at 100 has exactly qty 2.
    processLine(ob, "510	L	B	2	100	A");
    // clang-format off
    n.Verify({"CreateOrder Accepted 510 2 2",
              "6 510 FilledComplete FilledComplete 2 100"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(510));
}

// Limit buy AoN: insufficient liquidity at or below the limit → no trades, order rests.
TEST_F(LimitOrderTest, TestAoN_LimitBuy_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Buy AoN at 100 for qty 3 – ask at 100 has only qty 2, no other ask <= 100.
    processLine(ob, "511	L	B	3	100	A");
    n.Verify({"CreateOrder Accepted 511 3 3"});
    ASSERT_TRUE(ob->hasOrder(511));
}

// Limit sell AoN spanning multiple bid price levels → fully executes.
TEST_F(LimitOrderTest, TestAoN_LimitSell_MultiLevel_CanFill) {
    addDepth(ob);
    n.Reset();

    // Sell AoN at 80 for qty 4 – bids: 90(2) + 80(2) = 4 → enough.
    processLine(ob, "502	L	S	4	80	A");
    // clang-format off
    n.Verify({"CreateOrder Accepted 502 4 4",
              "5 502 FilledComplete FilledPartial 2 90",
              "4 502 FilledComplete FilledComplete 2 80"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(502));
}

// Limit sell AoN spanning multiple bid levels but one level short → no trades, order rests.
TEST_F(LimitOrderTest, TestAoN_LimitSell_MultiLevel_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Sell AoN at 90 for qty 3 – only 2 at bid 90 (no other bid >= 90) → cannot fill.
    processLine(ob, "503	L	S	3	90	A");
    n.Verify({"CreateOrder Accepted 503 3 3"});
    ASSERT_TRUE(ob->hasOrder(503));
}

// Limit buy AoN spanning multiple ask price levels → fully executes.
TEST_F(LimitOrderTest, TestAoN_LimitBuy_MultiLevel_CanFill) {
    addDepth(ob);
    n.Reset();

    // Buy AoN at 110 for qty 4 – asks: 100(2) + 110(2) = 4 → enough.
    processLine(ob, "512	L	B	4	110	A");
    // clang-format off
    n.Verify({"CreateOrder Accepted 512 4 4",
              "6 512 FilledComplete FilledPartial 2 100",
              "7 512 FilledComplete FilledComplete 2 110"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(512));
}

// Limit buy AoN: limit price falls between two ask levels, not enough fillable volume.
TEST_F(LimitOrderTest, TestAoN_LimitBuy_MultiLevel_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Buy AoN at 100 for qty 3 – only 2 available at or below 100 → cannot fill.
    processLine(ob, "513	L	B	3	100	A");
    n.Verify({"CreateOrder Accepted 513 3 3"});
    ASSERT_TRUE(ob->hasOrder(513));
}

// ──────────────────────────────────────────────────────────────────────────────
// AoN – market orders
// ──────────────────────────────────────────────────────────────────────────────

// Market sell AoN: enough bids in the book → fully executes.
TEST_F(LimitOrderTest, TestAoN_MarketSell_CanFill) {
    addDepth(ob);
    n.Reset();

    // 10 bids in the book (5 levels × qty 2). Market sell AoN for qty 6 → can fill.
    processLine(ob, "600	M	S	6	0	A");
    // clang-format off
    n.Verify({"CreateOrder Accepted 600 6 6",
              "5 600 FilledComplete FilledPartial 2 90",
              "4 600 FilledComplete FilledPartial 2 80",
              "3 600 FilledComplete FilledComplete 2 70"});
    // clang-format on
}

// Market sell AoN: not enough bids → no execution.
TEST_F(LimitOrderTest, TestAoN_MarketSell_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Total bid volume = 10; requesting 11 → cannot fill.
    processLine(ob, "601	M	S	11	0	A");
    n.Verify({"CreateOrder Accepted 601 11 11"});
}

// Market buy AoN: enough asks → fully executes.
TEST_F(LimitOrderTest, TestAoN_MarketBuy_CanFill) {
    addDepth(ob);
    n.Reset();

    // 10 asks in the book. Market buy AoN for qty 4 → fills 100(2) + 110(2).
    processLine(ob, "610	M	B	4	0	A");
    // clang-format off
    n.Verify({"CreateOrder Accepted 610 4 4",
              "6 610 FilledComplete FilledPartial 2 100",
              "7 610 FilledComplete FilledComplete 2 110"});
    // clang-format on
}

// Market buy AoN: more than total ask volume → no execution.
TEST_F(LimitOrderTest, TestAoN_MarketBuy_CannotFill) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "611	M	B	11	0	A");
    n.Verify({"CreateOrder Accepted 611 11 11"});
}

// ──────────────────────────────────────────────────────────────────────────────
// FoK – Fill or Kill (AoN + IoC: must fill entirely, never rests)
// ──────────────────────────────────────────────────────────────────────────────

// Limit sell FoK: enough bid liquidity at limit price → fills completely, no book entry.
TEST_F(LimitOrderTest, TestFoK_LimitSell_CanFill) {
    addDepth(ob);
    n.Reset();

    // Sell FoK at 90 for qty 2 – bid at 90 has qty 2.
    processLine(ob, "700	L	S	2	90	F");
    // clang-format off
    n.Verify({"CreateOrder Accepted 700 2 2",
              "5 700 FilledComplete FilledComplete 2 90"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(700));
}

// Limit sell FoK: not enough liquidity → no trades AND order is killed (not in book).
TEST_F(LimitOrderTest, TestFoK_LimitSell_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Sell FoK at 90 for qty 3 – only 2 available at >= 90 → kill.
    processLine(ob, "701	L	S	3	90	F");
    n.Verify({"CreateOrder Accepted 701 3 3"});
    // FoK must NOT rest in the book.
    ASSERT_FALSE(ob->hasOrder(701));
}

// Limit buy FoK: enough ask liquidity at limit price → fills completely, no book entry.
TEST_F(LimitOrderTest, TestFoK_LimitBuy_CanFill) {
    addDepth(ob);
    n.Reset();

    // Buy FoK at 100 for qty 2 – ask at 100 has qty 2.
    processLine(ob, "710	L	B	2	100	F");
    // clang-format off
    n.Verify({"CreateOrder Accepted 710 2 2",
              "6 710 FilledComplete FilledComplete 2 100"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(710));
}

// Limit buy FoK: not enough liquidity → no trades AND order is killed.
TEST_F(LimitOrderTest, TestFoK_LimitBuy_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Buy FoK at 100 for qty 3 – only 2 available at <= 100 → kill.
    processLine(ob, "711	L	B	3	100	F");
    n.Verify({"CreateOrder Accepted 711 3 3"});
    ASSERT_FALSE(ob->hasOrder(711));
}

// Limit sell FoK spanning multiple bid price levels → fills all, no book entry.
TEST_F(LimitOrderTest, TestFoK_LimitSell_MultiLevel_CanFill) {
    addDepth(ob);
    n.Reset();

    // Sell FoK at 70 for qty 6 – bids: 90(2) + 80(2) + 70(2) = 6.
    processLine(ob, "702	L	S	6	70	F");
    // clang-format off
    n.Verify({"CreateOrder Accepted 702 6 6",
              "5 702 FilledComplete FilledPartial 2 90",
              "4 702 FilledComplete FilledPartial 2 80",
              "3 702 FilledComplete FilledComplete 2 70"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(702));
}

// Limit sell FoK spanning multiple bid levels but one short → no trades, order killed.
TEST_F(LimitOrderTest, TestFoK_LimitSell_MultiLevel_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Sell FoK at 70 for qty 7 – max available at >= 70 is 6 → kill.
    processLine(ob, "703	L	S	7	70	F");
    n.Verify({"CreateOrder Accepted 703 7 7"});
    ASSERT_FALSE(ob->hasOrder(703));
}

// Limit buy FoK spanning multiple ask price levels → fills all, no book entry.
TEST_F(LimitOrderTest, TestFoK_LimitBuy_MultiLevel_CanFill) {
    addDepth(ob);
    n.Reset();

    // Buy FoK at 120 for qty 6 – asks: 100(2) + 110(2) + 120(2) = 6.
    processLine(ob, "712	L	B	6	120	F");
    // clang-format off
    n.Verify({"CreateOrder Accepted 712 6 6",
              "6 712 FilledComplete FilledPartial 2 100",
              "7 712 FilledComplete FilledPartial 2 110",
              "8 712 FilledComplete FilledComplete 2 120"});
    // clang-format on
    ASSERT_FALSE(ob->hasOrder(712));
}

// Limit buy FoK spanning multiple ask levels but one short → no trades, order killed.
TEST_F(LimitOrderTest, TestFoK_LimitBuy_MultiLevel_CannotFill) {
    addDepth(ob);
    n.Reset();

    // Buy FoK at 120 for qty 7 – max available at <= 120 is 6 → kill.
    processLine(ob, "713	L	B	7	120	F");
    n.Verify({"CreateOrder Accepted 713 7 7"});
    ASSERT_FALSE(ob->hasOrder(713));
}

// ──────────────────────────────────────────────────────────────────────────────
// FoK – market orders
// ──────────────────────────────────────────────────────────────────────────────

// Market sell FoK: enough bids → fully executes, order is never in the book.
TEST_F(LimitOrderTest, TestFoK_MarketSell_CanFill) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "800	M	S	4	0	F");
    // clang-format off
    n.Verify({"CreateOrder Accepted 800 4 4",
              "5 800 FilledComplete FilledPartial 2 90",
              "4 800 FilledComplete FilledComplete 2 80"});
    // clang-format on
}

// Market sell FoK: total bid volume insufficient → no trades.
TEST_F(LimitOrderTest, TestFoK_MarketSell_CannotFill) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "801	M	S	11	0	F");
    n.Verify({"CreateOrder Accepted 801 11 11"});
}

// Market buy FoK: enough asks → fully executes.
TEST_F(LimitOrderTest, TestFoK_MarketBuy_CanFill) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "810	M	B	4	0	F");
    // clang-format off
    n.Verify({"CreateOrder Accepted 810 4 4",
              "6 810 FilledComplete FilledPartial 2 100",
              "7 810 FilledComplete FilledComplete 2 110"});
    // clang-format on
}

// Market buy FoK: total ask volume insufficient → no trades.
TEST_F(LimitOrderTest, TestFoK_MarketBuy_CannotFill) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "811	M	B	11	0	F");
    n.Verify({"CreateOrder Accepted 811 11 11"});
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
