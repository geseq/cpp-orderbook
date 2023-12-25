#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "util.cpp"

class LimitOrderTest : public ::testing::Test {
   protected:
    Notification n;
    std::shared_ptr<OrderBook<Notification>> ob;
    std::atomic<uint64_t> tok = 0;

    void SetUp() override {
        tok = 0;
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
        Decimal trigPrice = parts[5];
        Flag flag = orderbook::Flag::None;
        if (parts[6] == "A") {
            flag = orderbook::AoN;
        } else if (parts[6] == "I") {
            flag = orderbook::IoC;
        } else if (parts[6] == "F") {
            flag = orderbook::FoK;
        }

        ob->addOrder(++tok, oid, type, side, qty, price, trigPrice, flag);
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
1	L	B	2	50	0	N
2	L	B	2	60	0	N
3	L	B	2	70	0	N
4	L	B	2	80	0	N
5	L	B	2	90	0	N
6	L	S	2	100	0	N
7	L	S	2	110	0	N
8	L	S	2	120	0	N
9	L	S	2	130	0	N
10	L	S	2	140	0	N
)";

        processOrders(ob, depth, prefix);
    }
};

TEST_F(LimitOrderTest, TestLimitOrder_Create) {
    for (int i = 50; i < 100; i += 10) {
        n.Reset();
        processLine(ob, std::to_string(i) + "	L	B	2	" + std::to_string(i) + "	0	N");
        n.Verify({"CreateOrder Accepted " + std::to_string(i) + " 2"});
    }

    for (int i = 100; i < 150; i += 10) {
        n.Reset();
        processLine(ob, std::to_string(i) + "	L	S	2	" + std::to_string(i) + "	0	N");
        n.Verify({"CreateOrder Accepted " + std::to_string(i) + " 2"});
    }

    ASSERT_FALSE(ob->hasOrder(999));
    ASSERT_TRUE(ob->hasOrder(100));
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateBuy) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "1100	L	B	1	100	0	N");
    n.Verify({"CreateOrder Accepted 1100 1", "6 1100 FilledPartial FilledComplete 1 100"});

    n.Reset();
    processLine(ob, "1150	L	B	10	150	0	N");
    // clang-format off
    n.Verify({"CreateOrder Accepted 1150 10", 
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
    processLine(ob, "170	L	S	0	40	0	N");
    n.Verify({"CreateOrder Rejected 170 0 ErrInvalidQty"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateWithZeroPrice) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "170	L	S	10	0	0	N");
    n.Verify({"CreateOrder Rejected 170 0 ErrInvalidPrice"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateAndCancel) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "170	L	S	10	1000	0	N");
    ob->cancelOrder(++tok, 170);
    // clang-format off
    n.Verify({"CreateOrder Accepted 170 10", 
            "CancelOrder Canceled 170 10"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_CancelNonExistent) {
    addDepth(ob);

    n.Reset();
    ob->cancelOrder(++tok, 170);
    tok++;
    n.Verify({"CancelOrder Rejected 170 0 ErrOrderNotExists"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateIOCWithNoMatches) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "300	L	S	1	200	0	I");
    n.Verify({"CreateOrder Accepted 300 1"});
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateIOCWithMatches) {
    return;  // TODO
    addDepth(ob);

    n.Reset();
    processLine(ob, "300	L	S	1	90	0	I");
    // clang-format off
    n.Verify({"CreateOrder Accepted 300 1", 
            "5 300 FilledPartial FilledComplete 1 90"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_CreateSell) {
    addDepth(ob);

    n.Reset();
    processLine(ob, "340	L	S	11	40	0	N");
    processLine(ob, "343	L	S	11	1	0	I");

    // clang-format off
    n.Verify({"CreateOrder Accepted 340 11", 
              "5 340 FilledComplete FilledPartial 2 90", 
              "4 340 FilledComplete FilledPartial 2 80", 
              "3 340 FilledComplete FilledPartial 2 70", 
              "2 340 FilledComplete FilledPartial 2 60", 
              "1 340 FilledComplete FilledPartial 2 50", 
              "CreateOrder Accepted 343 11"});

    // clang-format on
}

TEST_F(LimitOrderTest, TestLimitOrder_ClearSellBestPriceFirst) {
    addDepth(ob);
    n.Reset();

    processLine(ob, "900	L	B	11	1	0	N");
    processLine(ob, "901	L	S	11	1	0	N");

    ASSERT_FALSE(n.hasError());
    // clang-format off
    n.Verify({"CreateOrder Accepted 900 11", 
              "CreateOrder Accepted 901 11",
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

    processLine(ob, "800	M	B	3	0	0	N");
    processLine(ob, "801	M	B	0	0	0	N");
    processLine(ob, "802	M	S	12	0	0	N");
    processLine(ob, "803	M	B	12	0	0	A");
    processLine(ob, "804	M	B	12	0	0	N");

    // clang-format off
    n.Verify({"CreateOrder Accepted 800 3",
              "6 800 FilledComplete FilledPartial 2 100",
              "7 800 FilledPartial FilledComplete 1 110",
              "CreateOrder Rejected 801 0 ErrInvalidQty",
              "CreateOrder Accepted 802 12",
              "5 802 FilledComplete FilledPartial 2 90",
              "4 802 FilledComplete FilledPartial 2 80",
              "3 802 FilledComplete FilledPartial 2 70",
              "2 802 FilledComplete FilledPartial 2 60",
              "1 802 FilledComplete FilledPartial 2 50",
              "CreateOrder Accepted 803 12",
              "CreateOrder Accepted 804 12",
              "7 804 FilledComplete FilledPartial 1 110",
              "8 804 FilledComplete FilledPartial 2 120",
              "9 804 FilledComplete FilledPartial 2 130",
              "10 804 FilledComplete FilledPartial 2 140"});
    // clang-format on
}

TEST_F(LimitOrderTest, TestMarketProcess_PriceLevel_FIFO) {
    addDepth(ob, 0);
    addDepth(ob, 1);
    n.Reset();

    processLine(ob, "801	M	B	6	0	0	N");

    // clang-format off
        n.Verify({"CreateOrder Accepted 801 6",
                  "6 801 FilledComplete FilledPartial 2 100",
                  "16 801 FilledComplete FilledPartial 2 100",
                  "7 801 FilledComplete FilledComplete 2 110"});
    // clang-format on
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
