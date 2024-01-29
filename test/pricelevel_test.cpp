#include <gtest/gtest.h>

#include <memory>

#include "util.cpp"

namespace orderbook {

namespace test {

class PriceLevelTest : public ::testing::Test {
   protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(PriceLevelTest, TestPriceLevel) {
    PriceLevel<PriceType::Bid> bidLevel(10);

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

    ASSERT_EQ(&tree.begin()->order_list().front(), o2.get()) << "Invalid price levels: head of the first element does not match o1";
    ASSERT_EQ(&tree.begin()->order_list().back(), o2.get()) << "Invalid price levels: tail of the first element does not match o1";
    ASSERT_EQ(&tree.rbegin()->order_list().front(), o1.get()) << "Invalid price levels: head of the last element does not match o2";
    ASSERT_EQ(&tree.rbegin()->order_list().back(), o1.get()) << "Invalid price levels: tail of the last element does not match o2";

    bidLevel.remove(o1.get());

    {
        auto regularBegin = tree.begin();
        auto reverseBegin = tree.rbegin();

        // Dereference iterators to compare the values they point to
        ASSERT_EQ(&(*regularBegin), &(*reverseBegin));

        // Check if there's only one element
        ASSERT_TRUE(++regularBegin == tree.end());
    }

    // remove from the tree before o2 is destroyed
    bidLevel.remove(o2.get());
}

TEST_F(PriceLevelTest, TestPriceFinding) {
    PriceLevel<PriceType::Ask> askLevel(10);

    askLevel.append(new Order(1, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(130, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(2, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(170, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(3, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(100, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(4, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(160, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(5, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(140, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(6, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(120, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(7, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(150, 0), Decimal(uint64_t(0)), Flag::None));
    askLevel.append(new Order(8, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(110, 0), Decimal(uint64_t(0)), Flag::None));

    ASSERT_EQ(askLevel.volume(), Decimal(40, 0));

    ASSERT_EQ(askLevel.largestLessThan(Decimal(101, 0))->price(), Decimal(100, 0));
    ASSERT_EQ(askLevel.largestLessThan(Decimal(150, 0))->price(), Decimal(140, 0));
    ASSERT_EQ(askLevel.largestLessThan(Decimal(100, 0)), nullptr);

    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(169, 0))->price(), Decimal(170, 0));
    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(150, 0))->price(), Decimal(160, 0));
    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(170, 0)), nullptr);
}

TEST_F(PriceLevelTest, TestStopQueuePriceFinding) {
    PriceLevel<PriceType::TriggerUnder> trigLevel(10);

    trigLevel.append(new Order(1, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(10, 0), Decimal(130, 0), Flag::None));
    trigLevel.append(new Order(2, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(20, 0), Decimal(170, 0), Flag::None));
    trigLevel.append(new Order(3, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(30, 0), Decimal(100, 0), Flag::None));
    trigLevel.append(new Order(4, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(40, 0), Decimal(160, 0), Flag::None));
    trigLevel.append(new Order(5, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(50, 0), Decimal(140, 0), Flag::None));
    trigLevel.append(new Order(6, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(60, 0), Decimal(120, 0), Flag::None));
    trigLevel.append(new Order(7, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(70, 0), Decimal(150, 0), Flag::None));
    trigLevel.append(new Order(8, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(80, 0), Decimal(110, 0), Flag::None));

    ASSERT_EQ(trigLevel.volume(), Decimal(40, 0));

    std::cout << 1 << std::endl;
    ASSERT_EQ(trigLevel.largestLessThan(Decimal(101, 0))->price(), Decimal(100, 0));
    std::cout << 2 << std::endl;
    ASSERT_EQ(trigLevel.largestLessThan(Decimal(150, 0))->price(), Decimal(140, 0));
    ASSERT_EQ(trigLevel.largestLessThan(Decimal(100, 0)), nullptr);

    ASSERT_EQ(trigLevel.smallestGreaterThan(Decimal(169, 0))->price(), Decimal(170, 0));
    ASSERT_EQ(trigLevel.smallestGreaterThan(Decimal(150, 0))->price(), Decimal(160, 0));
    ASSERT_EQ(trigLevel.smallestGreaterThan(Decimal(170, 0)), nullptr);
}

}  // namespace test
}  // namespace orderbook

