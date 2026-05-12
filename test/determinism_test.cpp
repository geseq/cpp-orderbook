#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "util.cpp"

class DeterminismTest : public ::testing::Test {
   protected:
    struct Action {
        enum class Kind : uint8_t { Add, Cancel, SetMatching };
        Kind kind;
        OrderID id = 0;
        Type type = Type::Limit;
        Side side = Side::Buy;
        Decimal qty = Decimal(0, 0);
        Decimal price = Decimal(0, 0);
        Flag flag = Flag::None;
        bool matching = true;
    };

    static void applyAction(const Action& action, const std::shared_ptr<OrderBook<Notification>>& localOb, uint64_t& seq) {
        if (action.kind == Action::Kind::SetMatching) {
            localOb->setMatching(action.matching);
        } else if (action.kind == Action::Kind::Cancel) {
            localOb->cancelOrder(action.id);
        } else {
            localOb->addOrder(action.id, ++seq, action.type, action.side, action.qty, action.price, action.flag);
        }
    }

    static std::tuple<std::vector<std::string>, std::string, Decimal> runSequence(const std::vector<Action>& actions) {
        Notification notification;
        auto localOb = std::make_shared<orderbook::OrderBook<Notification>>(notification);
        uint64_t seq = 0;
        for (const auto& action : actions) {
            applyAction(action, localOb, seq);
        }
        return std::tuple{notification.Strings(), localOb->toString(), localOb->last_price};
    }

    static bool hasExactReport(const std::vector<std::string>& reports, const std::string& expected) {
        return std::find(reports.begin(), reports.end(), expected) != reports.end();
    }

    static bool hasReportContaining(const std::vector<std::string>& reports, const std::string& token) {
        for (const auto& report : reports) {
            if (report.find(token) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static bool hasActionKind(const std::vector<Action>& actions, Action::Kind expected) {
        return std::any_of(actions.begin(), actions.end(), [expected](const Action& action) { return action.kind == expected; });
    }

    static bool hasType(const std::vector<Action>& actions, Type expected) {
        return std::any_of(actions.begin(), actions.end(), [expected](const Action& action) { return action.kind == Action::Kind::Add && action.type == expected; });
    }

    static bool hasSide(const std::vector<Action>& actions, Side expected) {
        return std::any_of(actions.begin(), actions.end(), [expected](const Action& action) { return action.kind == Action::Kind::Add && action.side == expected; });
    }

    static bool hasFlag(const std::vector<Action>& actions, Flag expected) {
        return std::any_of(actions.begin(), actions.end(), [expected](const Action& action) { return action.kind == Action::Kind::Add && action.flag == expected; });
    }

    static std::vector<Action> marketAActions() {
        return {
            {Action::Kind::Add, 1, Type::Limit, Side::Buy, Decimal(0, 0), Decimal(100, 0), Flag::None},
            {Action::Kind::Add, 2, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(90, 0), Flag::None},
            {Action::Kind::Cancel, 2},
            {Action::Kind::Cancel, 2},
            {Action::Kind::SetMatching, 0, Type::Limit, Side::Buy, Decimal(0, 0), Decimal(0, 0), Flag::None, false},
            {Action::Kind::Add, 3, Type::Market, Side::Buy, Decimal(1, 0), Decimal(0, 0), Flag::None},
            {Action::Kind::Add, 4, Type::Limit, Side::Sell, Decimal(1, 0), Decimal(80, 0), Flag::None},
            {Action::Kind::Add, 5, Type::Limit, Side::Buy, Decimal(1, 0), Decimal(70, 0), Flag::None},
            {Action::Kind::SetMatching, 0, Type::Limit, Side::Buy, Decimal(0, 0), Decimal(0, 0), Flag::None, true},
            {Action::Kind::Add, 6, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(90, 0), Flag::None},
            {Action::Kind::Add, 6, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(90, 0), Flag::None},
            {Action::Kind::Add, 7, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(100, 0), Flag::None},
            {Action::Kind::Add, 8, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(110, 0), Flag::None},
            {Action::Kind::Add, 9, Type::Limit, Side::Buy, Decimal(1, 0), Decimal(0, 0), Flag::None},
            {Action::Kind::Add, 10, Type::Limit, Side::Buy, Decimal(3, 0), Decimal(100, 0), Flag::None},
            {Action::Kind::Add, 11, Type::Limit, Side::Sell, Decimal(5, 0), Decimal(90, 0), Flag::IoC},
            {Action::Kind::Add, 12, Type::Limit, Side::Buy, Decimal(3, 0), Decimal(100, 0), Flag::AoN},
            {Action::Kind::Add, 13, Type::Limit, Side::Buy, Decimal(4, 0), Decimal(120, 0), Flag::FoK},
            {Action::Kind::Add, 14, Type::Market, Side::Sell, Decimal(3, 0), Decimal(0, 0), Flag::None},
            {Action::Kind::Add, 15, Type::Market, Side::Buy, Decimal(5, 0), Decimal(0, 0), Flag::AoN},
            {Action::Kind::Add, 16, Type::Market, Side::Buy, Decimal(2, 0), Decimal(0, 0), Flag::FoK},
            {Action::Kind::Cancel, 8},
            {Action::Kind::Cancel, 999},
        };
    }

    static std::vector<Action> marketBActions() {
        return {
            {Action::Kind::Add, 1001, Type::Limit, Side::Sell, Decimal(0, 0), Decimal(100, 0), Flag::None},
            {Action::Kind::Add, 1002, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(110, 0), Flag::None},
            {Action::Kind::Cancel, 1002},
            {Action::Kind::Cancel, 1002},
            {Action::Kind::SetMatching, 0, Type::Limit, Side::Buy, Decimal(0, 0), Decimal(0, 0), Flag::None, false},
            {Action::Kind::Add, 1003, Type::Market, Side::Sell, Decimal(1, 0), Decimal(0, 0), Flag::None},
            {Action::Kind::Add, 1004, Type::Limit, Side::Buy, Decimal(1, 0), Decimal(120, 0), Flag::None},
            {Action::Kind::Add, 1005, Type::Limit, Side::Sell, Decimal(1, 0), Decimal(130, 0), Flag::None},
            {Action::Kind::SetMatching, 0, Type::Limit, Side::Buy, Decimal(0, 0), Decimal(0, 0), Flag::None, true},
            {Action::Kind::Add, 1006, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(110, 0), Flag::None},
            {Action::Kind::Add, 1006, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(110, 0), Flag::None},
            {Action::Kind::Add, 1007, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(100, 0), Flag::None},
            {Action::Kind::Add, 1008, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(90, 0), Flag::None},
            {Action::Kind::Add, 1009, Type::Limit, Side::Sell, Decimal(1, 0), Decimal(0, 0), Flag::None},
            {Action::Kind::Add, 1010, Type::Limit, Side::Sell, Decimal(3, 0), Decimal(100, 0), Flag::None},
            {Action::Kind::Add, 1011, Type::Limit, Side::Buy, Decimal(5, 0), Decimal(110, 0), Flag::IoC},
            {Action::Kind::Add, 1012, Type::Limit, Side::Sell, Decimal(3, 0), Decimal(100, 0), Flag::AoN},
            {Action::Kind::Add, 1013, Type::Limit, Side::Sell, Decimal(4, 0), Decimal(80, 0), Flag::FoK},
            {Action::Kind::Add, 1014, Type::Market, Side::Buy, Decimal(3, 0), Decimal(0, 0), Flag::None},
            {Action::Kind::Add, 1015, Type::Market, Side::Sell, Decimal(5, 0), Decimal(0, 0), Flag::AoN},
            {Action::Kind::Add, 1016, Type::Market, Side::Sell, Decimal(2, 0), Decimal(0, 0), Flag::FoK},
            {Action::Kind::Cancel, 1008},
            {Action::Kind::Cancel, 1999},
        };
    }
};

TEST_F(DeterminismTest, ReplayProducesSameExecutionTraceAndBookState) {
    const auto actions = marketAActions();
    const auto [reportsA, bookA, lastPriceA] = runSequence(actions);
    const auto [reportsB, bookB, lastPriceB] = runSequence(actions);
    const auto [reportsC, bookC, lastPriceC] = runSequence(actions);

    ASSERT_EQ(reportsA, reportsB);
    ASSERT_EQ(reportsA, reportsC);
    ASSERT_TRUE(hasExactReport(reportsA, "CreateOrder Rejected 1 0 0 ErrInvalidQty"));
    ASSERT_TRUE(hasExactReport(reportsA, "CancelOrder Canceled 2 2 2"));
    ASSERT_TRUE(hasExactReport(reportsA, "CancelOrder Rejected 2 0 0 ErrOrderNotExists"));
    ASSERT_EQ(bookA, bookB);
    ASSERT_EQ(bookA, bookC);
    ASSERT_EQ(lastPriceA, lastPriceB);
    ASSERT_EQ(lastPriceA, lastPriceC);
}

TEST_F(DeterminismTest, IndependentBooksStayIsolated) {
    Notification n1;
    Notification n2;
    auto ob1 = std::make_shared<orderbook::OrderBook<Notification>>(n1);
    auto ob2 = std::make_shared<orderbook::OrderBook<Notification>>(n2);

    ob1->addOrder(1000, 1, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(100, 0), Flag::None);
    ob1->addOrder(1001, 2, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(100, 0), Flag::None);

    ob2->addOrder(2000, 1, Type::Limit, Side::Buy, Decimal(2, 0), Decimal(100, 0), Flag::None);
    ob2->addOrder(2001, 2, Type::Limit, Side::Sell, Decimal(2, 0), Decimal(100, 0), Flag::None);

    n1.Verify({"CreateOrder Accepted 1000 2 2", "CreateOrder Accepted 1001 2 2", "1000 1001 FilledComplete FilledComplete 2 100"});
    n2.Verify({"CreateOrder Accepted 2000 2 2", "CreateOrder Accepted 2001 2 2", "2000 2001 FilledComplete FilledComplete 2 100"});
    ASSERT_EQ(ob1->toString(), ob2->toString());
}

TEST_F(DeterminismTest, RecoverAtEveryMidpointThenReplaySuffix) {
    const auto actions = marketAActions();

    for (size_t split = 0; split <= actions.size(); ++split) {
        uint64_t baselineSeq = 0;
        Notification baselineN;
        auto baselineOb = std::make_shared<orderbook::OrderBook<Notification>>(baselineN);
        for (size_t i = 0; i < split; ++i) {
            applyAction(actions[i], baselineOb, baselineSeq);
        }
        const auto snapshotBookState = baselineOb->toString();
        const auto snapshotLastPrice = baselineOb->last_price;

        baselineN.Reset();
        for (size_t i = split; i < actions.size(); ++i) {
            applyAction(actions[i], baselineOb, baselineSeq);
        }
        const auto baselineSuffixReports = baselineN.Strings();
        const auto baselineFinalBook = baselineOb->toString();
        const auto baselineFinalLastPrice = baselineOb->last_price;

        uint64_t recoveredSeq = 0;
        Notification recoveredN;
        auto recoveredOb = std::make_shared<orderbook::OrderBook<Notification>>(recoveredN);
        for (size_t i = 0; i < split; ++i) {
            applyAction(actions[i], recoveredOb, recoveredSeq);
        }

        ASSERT_EQ(recoveredOb->toString(), snapshotBookState) << "split=" << split;
        ASSERT_EQ(recoveredOb->last_price, snapshotLastPrice) << "split=" << split;

        recoveredN.Reset();
        for (size_t i = split; i < actions.size(); ++i) {
            applyAction(actions[i], recoveredOb, recoveredSeq);
        }

        ASSERT_EQ(recoveredN.Strings(), baselineSuffixReports) << "split=" << split;
        ASSERT_EQ(recoveredOb->toString(), baselineFinalBook) << "split=" << split;
        ASSERT_EQ(recoveredOb->last_price, baselineFinalLastPrice) << "split=" << split;
    }
}

TEST_F(DeterminismTest, TwoCopiesPerMarketRemainDeterministicWithDifferentInterleavedMarkets) {
    const auto marketA = marketAActions();
    const auto marketB = marketBActions();

    ASSERT_TRUE(hasActionKind(marketA, Action::Kind::Cancel));
    ASSERT_TRUE(hasActionKind(marketA, Action::Kind::SetMatching));
    ASSERT_TRUE(hasActionKind(marketB, Action::Kind::Cancel));
    ASSERT_TRUE(hasActionKind(marketB, Action::Kind::SetMatching));
    ASSERT_TRUE(hasType(marketA, Type::Limit));
    ASSERT_TRUE(hasType(marketA, Type::Market));
    ASSERT_TRUE(hasType(marketB, Type::Limit));
    ASSERT_TRUE(hasType(marketB, Type::Market));
    ASSERT_TRUE(hasSide(marketA, Side::Buy));
    ASSERT_TRUE(hasSide(marketA, Side::Sell));
    ASSERT_TRUE(hasSide(marketB, Side::Buy));
    ASSERT_TRUE(hasSide(marketB, Side::Sell));
    ASSERT_TRUE(hasFlag(marketA, Flag::None));
    ASSERT_TRUE(hasFlag(marketA, Flag::IoC));
    ASSERT_TRUE(hasFlag(marketA, Flag::AoN));
    ASSERT_TRUE(hasFlag(marketA, Flag::FoK));
    ASSERT_TRUE(hasFlag(marketB, Flag::None));
    ASSERT_TRUE(hasFlag(marketB, Flag::IoC));
    ASSERT_TRUE(hasFlag(marketB, Flag::AoN));
    ASSERT_TRUE(hasFlag(marketB, Flag::FoK));

    const auto [standaloneReportsA, standaloneBookA, standaloneLastPriceA] = runSequence(marketA);
    const auto [standaloneReportsB, standaloneBookB, standaloneLastPriceB] = runSequence(marketB);

    Notification nA1;
    Notification nA2;
    Notification nB1;
    Notification nB2;
    auto obA1 = std::make_shared<orderbook::OrderBook<Notification>>(nA1);
    auto obA2 = std::make_shared<orderbook::OrderBook<Notification>>(nA2);
    auto obB1 = std::make_shared<orderbook::OrderBook<Notification>>(nB1);
    auto obB2 = std::make_shared<orderbook::OrderBook<Notification>>(nB2);

    const size_t maxActions = std::max(marketA.size(), marketB.size());
    uint64_t seqA1 = 0, seqA2 = 0, seqB1 = 0, seqB2 = 0;
    for (size_t i = 0; i < maxActions; ++i) {
        if (i < marketA.size()) {
            applyAction(marketA[i], obA1, seqA1);
            applyAction(marketA[i], obA2, seqA2);
        }
        if (i < marketB.size()) {
            applyAction(marketB[i], obB1, seqB1);
            applyAction(marketB[i], obB2, seqB2);
        }
    }

    ASSERT_EQ(nA1.Strings(), nA2.Strings());
    ASSERT_EQ(obA1->toString(), obA2->toString());
    ASSERT_EQ(obA1->last_price, obA2->last_price);

    ASSERT_EQ(nB1.Strings(), nB2.Strings());
    ASSERT_EQ(obB1->toString(), obB2->toString());
    ASSERT_EQ(obB1->last_price, obB2->last_price);

    ASSERT_EQ(nA1.Strings(), standaloneReportsA);
    ASSERT_EQ(obA1->toString(), standaloneBookA);
    ASSERT_EQ(obA1->last_price, standaloneLastPriceA);
    ASSERT_EQ(nB1.Strings(), standaloneReportsB);
    ASSERT_EQ(obB1->toString(), standaloneBookB);
    ASSERT_EQ(obB1->last_price, standaloneLastPriceB);

    ASSERT_TRUE(hasReportContaining(nA1.Strings(), "Filled"));
    ASSERT_TRUE(hasReportContaining(nA1.Strings(), "CreateOrder Rejected"));
    ASSERT_TRUE(hasReportContaining(nA1.Strings(), "CancelOrder Canceled"));
    ASSERT_TRUE(hasReportContaining(nA1.Strings(), "CancelOrder Rejected"));
    ASSERT_TRUE(hasReportContaining(nB1.Strings(), "Filled"));
    ASSERT_TRUE(hasReportContaining(nB1.Strings(), "CreateOrder Rejected"));
    ASSERT_TRUE(hasReportContaining(nB1.Strings(), "CancelOrder Canceled"));
    ASSERT_TRUE(hasReportContaining(nB1.Strings(), "CancelOrder Rejected"));
}
