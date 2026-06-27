#include <gtest/gtest.h>

#include <memory>

#include "rbtree_levels.hpp"
#include "util.cpp"

namespace orderbook {

namespace test {

class PriceLevelTest : public ::testing::Test {
   protected:
    void SetUp() override {}

    void TearDown() override {}
};

// Body templated over the LevelStore policy so the SAME assertions run against
// both backends (ArrayLevels and RbTreeLevels).
template <template <PriceType> class Levels>
void runTestPriceLevel() {
    // Tick-indexed config: base_fp=0, tick_fp=1e8 (one tick per 1.0), 256 ticks.
    // RbTreeLevels ignores the tick params and uses only pool_size.
    PriceLevel<PriceType::Bid, Levels<PriceType::Bid>> bidLevel(LevelStoreConfig{10, 0, 100000000, 256});

    auto o1 = std::make_shared<Order>(1, Type::Limit, Side::Buy, Decimal(10, 0), Decimal(10, 0), Flag::None);
    auto o2 = std::make_shared<Order>(2, Type::Limit, Side::Buy, Decimal(10, 0), Decimal(20, 0), Flag::None);

    // Empty book: no best level.
    ASSERT_EQ(bidLevel.getQueue(), nullptr);

    bidLevel.append(o1.get());

    {
        // Single level: it is the best, and there is nothing below it.
        auto* best = bidLevel.getQueue();
        ASSERT_NE(best, nullptr);
        ASSERT_EQ(&best->order_list().front(), o1.get());
        ASSERT_EQ(bidLevel.depth(), 1);
        ASSERT_EQ(bidLevel.len(), 1);
        ASSERT_EQ(bidLevel.largestLessThan(Decimal(10, 0)), nullptr);
    }

    bidLevel.append(o2.get());
    ASSERT_EQ(bidLevel.depth(), 2);
    ASSERT_EQ(bidLevel.len(), 2);

    // Best (bid = highest price) is the price-20 level holding o2.
    auto* bestLevel = bidLevel.getQueue();
    ASSERT_EQ(&bestLevel->order_list().front(), o2.get()) << "Invalid price levels: head of the best level does not match o2";
    ASSERT_EQ(&bestLevel->order_list().back(), o2.get()) << "Invalid price levels: tail of the best level does not match o2";

    // The next-lower occupied level holds o1 at price 10.
    auto* lowerLevel = bidLevel.largestLessThan(Decimal(20, 0));
    ASSERT_NE(lowerLevel, nullptr);
    ASSERT_EQ(&lowerLevel->order_list().front(), o1.get()) << "Invalid price levels: head of the lower level does not match o1";
    ASSERT_EQ(&lowerLevel->order_list().back(), o1.get()) << "Invalid price levels: tail of the lower level does not match o1";

    bidLevel.remove(o1.get());

    {
        // Only the price-20 level (o2) remains; it is the best with nothing below.
        auto* best = bidLevel.getQueue();
        ASSERT_NE(best, nullptr);
        ASSERT_EQ(&best->order_list().front(), o2.get());
        ASSERT_EQ(bidLevel.depth(), 1);
        ASSERT_EQ(bidLevel.len(), 1);
        ASSERT_EQ(bidLevel.largestLessThan(Decimal(20, 0)), nullptr);
    }

    // remove before o2 is destroyed
    bidLevel.remove(o2.get());
}

template <template <PriceType> class Levels>
void runTestPriceFinding() {
    // base_fp=0, tick_fp=1e8 (one tick per 1.0), 256 ticks covers prices up to 255.
    PriceLevel<PriceType::Ask, Levels<PriceType::Ask>> askLevel(LevelStoreConfig{10, 0, 100000000, 256});

    askLevel.append(new Order(1, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(130, 0), Flag::None));
    askLevel.append(new Order(2, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(170, 0), Flag::None));
    askLevel.append(new Order(3, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(100, 0), Flag::None));
    askLevel.append(new Order(4, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(160, 0), Flag::None));
    askLevel.append(new Order(5, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(140, 0), Flag::None));
    askLevel.append(new Order(6, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(120, 0), Flag::None));
    askLevel.append(new Order(7, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(150, 0), Flag::None));
    askLevel.append(new Order(8, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(110, 0), Flag::None));

    ASSERT_EQ(askLevel.volume(), Decimal(40, 0));

    ASSERT_EQ(askLevel.largestLessThan(Decimal(101, 0))->price(), Decimal(100, 0));
    ASSERT_EQ(askLevel.largestLessThan(Decimal(150, 0))->price(), Decimal(140, 0));
    ASSERT_EQ(askLevel.largestLessThan(Decimal(100, 0)), nullptr);

    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(169, 0))->price(), Decimal(170, 0));
    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(150, 0))->price(), Decimal(160, 0));
    ASSERT_EQ(askLevel.smallestGreaterThan(Decimal(170, 0)), nullptr);
}

TEST_F(PriceLevelTest, TestPriceLevel_Array) { runTestPriceLevel<ArrayLevels>(); }
TEST_F(PriceLevelTest, TestPriceLevel_RbTree) { runTestPriceLevel<RbTreeLevels>(); }

TEST_F(PriceLevelTest, TestPriceFinding_Array) { runTestPriceFinding<ArrayLevels>(); }
TEST_F(PriceLevelTest, TestPriceFinding_RbTree) { runTestPriceFinding<RbTreeLevels>(); }

}  // namespace test
}  // namespace orderbook
