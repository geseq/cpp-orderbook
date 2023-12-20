#include <gtest/gtest.h>

#include <memory>

#include "util.cpp"

namespace orderbook {

namespace test {

class PriceLevelTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(PriceLevelTest, TestPriceLevel) {
    PriceLevel<CmpLess> bidLevel(PriceType::Bid, 10);

    auto o1 = std::make_shared<Order>(1, Type::Limit, Side::Buy, Decimal(10, 0), Decimal(10, 0), Decimal(uint64_t(0)), Flag::None);
    auto o2 = std::make_shared<Order>(2, Type::Limit, Side::Buy, Decimal(10, 0), Decimal(20, 0), Decimal(uint64_t(0)), Flag::None);

    auto& tree = bidLevel.price_tree();

    ASSERT_EQ(tree.begin(), tree.end());
    ASSERT_EQ(tree.rbegin(), tree.rend());

    bidLevel.append(o1.get());

    {
        auto regularBegin = tree.begin();
        auto reverseBegin = tree.rbegin();

        // Dereference iterators to compare the values they point to
        ASSERT_EQ(&(*regularBegin), &(*reverseBegin));

        // Check if there's only one element
        ASSERT_TRUE(++regularBegin == tree.end());
    }

    bidLevel.append(o2.get());
    ASSERT_EQ(bidLevel.depth(), 2);
    ASSERT_EQ(bidLevel.len(), 2);

    if (tree.begin()->head() != o1.get() || tree.begin()->tail() != o1.get() || tree.rbegin()->head() != o2.get() || tree.rbegin()->tail() != o2.get()) {
        FAIL() << "invalid price levels";
    }

    bidLevel.remove(o1);

    {
        auto regularBegin = tree.begin();
        auto reverseBegin = tree.rbegin();

        // Dereference iterators to compare the values they point to
        ASSERT_EQ(&(*regularBegin), &(*reverseBegin));

        // Check if there's only one element
        ASSERT_TRUE(++regularBegin == tree.end());
    }
}

TEST_F(PriceLevelTest, TestPriceFinding) {
    PriceLevel<CmpLess> askLevel(PriceType::Ask, 10);

    askLevel.append(new Order(1, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(130, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(2, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(170, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(3, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(100, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(4, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(160, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(5, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(140, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(6, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(120, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(7, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(150, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(8, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(110, 0), Decimal(uint64_t(0)), Flag::None));

    ASSERT_EQ(askLevel.volume(), Decimal(40, 0));

    ASSERT_EQ(askLevel.LargestLessThan(Decimal(101, 0))->price(), Decimal(100, 0));
    ASSERT_EQ(askLevel.LargestLessThan(Decimal(150, 0))->price(), Decimal(140, 0));
    ASSERT_EQ(askLevel.LargestLessThan(Decimal(100, 0)), nullptr);

    ASSERT_EQ(askLevel.SmallestGreaterThan(Decimal(169, 0))->price(), Decimal(170, 0));
    ASSERT_EQ(askLevel.SmallestGreaterThan(Decimal(150, 0))->price(), Decimal(160, 0));
    ASSERT_EQ(askLevel.SmallestGreaterThan(Decimal(170, 0)), nullptr);
}

TEST_F(PriceLevelTest, TestStopQueuePriceFinding) {
    PriceLevel<CmpLess> trigLevel(PriceType::Trigger, 10);

    trigLevel.append(new Order(1, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(10, 0), Decimal(130, 0), Flag::None));
    trigLevel.append(new Order(2, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(20, 0), Decimal(170, 0), Flag::None));
    trigLevel.append(new Order(3, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(30, 0), Decimal(100, 0), Flag::None));
    trigLevel.append(new Order(4, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(40, 0), Decimal(160, 0), Flag::None));
    trigLevel.append(new Order(5, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(50, 0), Decimal(140, 0), Flag::None));
    trigLevel.append(new Order(6, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(60, 0), Decimal(120, 0), Flag::None));
    trigLevel.append(new Order(7, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(70, 0), Decimal(150, 0), Flag::None));
    trigLevel.append(new Order(8, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(80, 0), Decimal(110, 0), Flag::None));

    ASSERT_EQ(trigLevel.volume(), Decimal(40, 0));

    ASSERT_EQ(trigLevel.LargestLessThan(Decimal(101, 0))->price(), Decimal(100, 0));
    ASSERT_EQ(trigLevel.LargestLessThan(Decimal(150, 0))->price(), Decimal(140, 0));
    ASSERT_EQ(trigLevel.LargestLessThan(Decimal(100, 0)), nullptr);

    ASSERT_EQ(trigLevel.SmallestGreaterThan(Decimal(169, 0))->price(), Decimal(170, 0));
    ASSERT_EQ(trigLevel.SmallestGreaterThan(Decimal(150, 0))->price(), Decimal(160, 0));
    ASSERT_EQ(trigLevel.SmallestGreaterThan(Decimal(170, 0)), nullptr);
}

}  // namespace test
}  // namespace orderbook

