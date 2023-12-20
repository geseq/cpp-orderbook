#include <gtest/gtest.h>

#include <memory>

#include "util.cpp"

namespace orderbook {

namespace test {

class OrderQueueTest : public ::testing::Test {
   protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(OrderQueueTest, TestOrderQueue) {
    Decimal price(100, 0);
    auto oq = std::make_unique<OrderQueue>(price);

    auto o1 = Order(1, Type::Limit, Side::Buy, Decimal(100, 0), Decimal(100, 0), Decimal(0, 0), Flag::None);
    auto o2 = Order(2, Type::Limit, Side::Buy, Decimal(100, 0), Decimal(100, 0), Decimal(0, 0), Flag::None);

    oq->append(&o1);
    oq->append(&o2);

    auto* head = oq->head();
    auto* tail = oq->tail();

    ASSERT_NE(head, nullptr);
    ASSERT_NE(tail, nullptr);

    ASSERT_EQ(oq->totalQty(), Decimal(200, 0));

    ASSERT_EQ(head, &o1);
    ASSERT_EQ(tail, &o2);

    ASSERT_EQ(oq->head()->next, oq->tail());
    ASSERT_EQ(oq->tail()->prev, head);
    ASSERT_EQ(oq->head()->prev, nullptr);
    ASSERT_EQ(oq->tail()->next, nullptr);

    oq->remove(&o1);

    ASSERT_EQ(oq->head(), &o2);
    ASSERT_EQ(oq->head(), oq->tail());
    ASSERT_EQ(oq->head()->next, nullptr);
    ASSERT_EQ(oq->tail()->prev, nullptr);
    ASSERT_EQ(oq->head()->prev, nullptr);
    ASSERT_EQ(oq->tail()->next, nullptr);

    ASSERT_EQ(oq->totalQty(), Decimal(100, 0));
}

}  // namespace test
}  // namespace orderbook

