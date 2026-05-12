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

    auto o1 = std::make_shared<Order>(1, 0, Type::Limit, Side::Buy, Decimal(10, 0), Decimal(10, 0), Flag::None);
    auto o2 = std::make_shared<Order>(2, 0, Type::Limit, Side::Buy, Decimal(10, 0), Decimal(20, 0), Flag::None);

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

    std::vector<std::unique_ptr<Order>> orders;
    auto addOrder = [&](OrderID id, Decimal qty, Decimal price) {
        orders.push_back(std::make_unique<Order>(id, 0, Type::Limit, Side::Sell, qty, price, Flag::None));
        askLevel.append(orders.back().get());
    };

    addOrder(1, Decimal(5, 0), Decimal(130, 0));
    addOrder(2, Decimal(5, 0), Decimal(170, 0));
    addOrder(3, Decimal(5, 0), Decimal(100, 0));
    addOrder(4, Decimal(5, 0), Decimal(160, 0));
    addOrder(5, Decimal(5, 0), Decimal(140, 0));
    addOrder(6, Decimal(5, 0), Decimal(120, 0));
    addOrder(7, Decimal(5, 0), Decimal(150, 0));
    addOrder(8, Decimal(5, 0), Decimal(110, 0));

    ASSERT_EQ(askLevel.volume(), Decimal(40, 0));

    ASSERT_EQ(askLevel.largestLessThan(Decimal(101, 0))->price(), Decimal(100, 0));
    ASSERT_EQ(askLevel.largestLessThan(Decimal(150, 0))->price(), Decimal(140, 0));
    ASSERT_EQ(askLevel.largestLessThan(Decimal(100, 0)), nullptr);

    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(169, 0))->price(), Decimal(170, 0));
    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(150, 0))->price(), Decimal(160, 0));
    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(170, 0)), nullptr);

    // Remove orders from the PriceLevel before they are destroyed
    for (auto& o : orders) {
        askLevel.remove(o.get());
    }
}

}  // namespace test
}  // namespace orderbook

