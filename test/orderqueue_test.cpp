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

    auto o1 = Order(1, Type::Limit, Side::Buy, Decimal(100, 0), Decimal(100, 0), Flag::None);
    auto o2 = Order(2, Type::Limit, Side::Buy, Decimal(100, 0), Decimal(100, 0), Flag::None);

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

TEST_F(OrderQueueTest, TestOrderQueue_ProcessUpdatesTotalQtyOnFullFill) {
    Decimal price(100, 0);
    auto oq = std::make_unique<OrderQueue>(price);

    auto o1 = Order(1, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);
    auto o2 = Order(2, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);

    oq->append(&o1);
    oq->append(&o2);

    const TradeNotification tn = [](OrderID makerOrderID, OrderID takerOrderID, OrderStatus makerOrderStatus, OrderStatus takerOrderStatus, Decimal matchedQty,
                                    Decimal matchedPrice) {};
    const PostOrderFill pf = [&oq, &o1, &o2](OrderID id) {
        if (id == 1) {
            oq->remove(&o1);
        } else if (id == 2) {
            oq->remove(&o2);
        }
    };

    auto qtyProcessed = oq->process(tn, pf, 900, Decimal(150, 0));

    EXPECT_EQ(qtyProcessed, Decimal(150, 0));
    EXPECT_EQ(oq->len(), 1);
    EXPECT_EQ(oq->totalQty(), Decimal(50, 0));

    // remove from container before destroying
    oq->remove(&o2);
}

TEST_F(OrderQueueTest, TestOrderQueue_ProcessUpdatesTotalQtyOnExactFill) {
    Decimal price(100, 0);
    auto oq = std::make_unique<OrderQueue>(price);

    auto o1 = Order(1, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);
    auto o2 = Order(2, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);

    oq->append(&o1);
    oq->append(&o2);

    const TradeNotification tn = [](OrderID makerOrderID, OrderID takerOrderID, OrderStatus makerOrderStatus, OrderStatus takerOrderStatus, Decimal matchedQty,
                                    Decimal matchedPrice) {};
    const PostOrderFill pf = [&oq, &o1, &o2](OrderID id) {
        if (id == 1) {
            oq->remove(&o1);
        } else if (id == 2) {
            oq->remove(&o2);
        }
    };

    auto qtyProcessed = oq->process(tn, pf, 901, Decimal(200, 0));

    EXPECT_EQ(qtyProcessed, Decimal(200, 0));
    EXPECT_EQ(oq->len(), 0);
    EXPECT_EQ(oq->totalQty(), Decimal(0, 0));
}

TEST_F(OrderQueueTest, TestOrderQueue_ProcessZeroesFilledOrderBeforePostFill) {
    Decimal price(100, 0);
    auto oq = std::make_unique<OrderQueue>(price);

    auto o1 = Order(1, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);
    auto o2 = Order(2, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);

    oq->append(&o1);
    oq->append(&o2);

    bool sawZeroQtyOnPostFill = false;
    const TradeNotification tn = [](OrderID makerOrderID, OrderID takerOrderID, OrderStatus makerOrderStatus, OrderStatus takerOrderStatus, Decimal matchedQty,
                                    Decimal matchedPrice) {};
    const PostOrderFill pf = [&oq, &o1, &o2, &sawZeroQtyOnPostFill](OrderID id) {
        if (id == 1) {
            sawZeroQtyOnPostFill = (o1.qty == Decimal(0, 0));
            oq->remove(&o1);
        } else if (id == 2) {
            oq->remove(&o2);
        }
    };

    auto qtyProcessed = oq->process(tn, pf, 902, Decimal(150, 0));

    EXPECT_EQ(qtyProcessed, Decimal(150, 0));
    EXPECT_TRUE(sawZeroQtyOnPostFill);
    EXPECT_EQ(oq->len(), 1);
    EXPECT_EQ(oq->totalQty(), Decimal(50, 0));

    // remove from container before destroying
    oq->remove(&o2);
}

TEST_F(OrderQueueTest, TestOrderQueue_ProcessUsesSnapshotForTradeNotificationAfterPostFill) {
    Decimal price(100, 0);
    auto oq = std::make_unique<OrderQueue>(price);

    auto o1 = Order(1, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);
    auto o2 = Order(2, Type::Limit, Side::Buy, Decimal(100, 0), price, Flag::None);

    oq->append(&o1);
    oq->append(&o2);

    OrderID makerOrderID = 0;
    Decimal matchedPrice(0, 0);
    const TradeNotification tn = [&makerOrderID, &matchedPrice](OrderID makerID, OrderID takerOrderID, OrderStatus makerOrderStatus,
                                                                OrderStatus takerOrderStatus, Decimal matchedQty, Decimal priceValue) {
        makerOrderID = makerID;
        matchedPrice = priceValue;
    };
    const PostOrderFill pf = [&oq, &o1](OrderID id) {
        if (id == 1) {
            oq->remove(&o1);
            o1.id = 999;
            o1.price = Decimal(999, 0);
        }
    };

    auto qtyProcessed = oq->process(tn, pf, 903, Decimal(100, 0));

    EXPECT_EQ(qtyProcessed, Decimal(100, 0));
    EXPECT_EQ(makerOrderID, 1);
    EXPECT_EQ(matchedPrice, Decimal(100, 0));
    EXPECT_EQ(oq->len(), 1);
    EXPECT_EQ(oq->totalQty(), Decimal(100, 0));

    // remove from container before destroying
    oq->remove(&o2);
}

}  // namespace orderbook::test
