#include <gtest/gtest.h>

#include <memory>

#include "util.cpp"

namespace orderbook::test {

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

    auto* head = &oq->order_list().front();
    auto* tail = &oq->order_list().back();

    ASSERT_NE(head, nullptr);
    ASSERT_NE(tail, nullptr);

    ASSERT_EQ(oq->totalQty(), Decimal(200, 0));
    ASSERT_EQ(oq->len(), 2);

    oq->remove(&o1);

    ASSERT_EQ(&oq->order_list().front(), &o2);
    ASSERT_EQ(oq->order_list().front(), oq->order_list().back());

    ASSERT_EQ(oq->len(), 1);
    ASSERT_EQ(oq->totalQty(), Decimal(100, 0));

    // remove from container before destroying
    oq->remove(&o2);
}

}  // namespace orderbook::test

