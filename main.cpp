#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <locale>kkkk
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "include/orderbook.hpp"
#include "include/types.hpp"

using orderbook::Decimal;
using orderbook::Flag;
using orderbook::MsgType;
using orderbook::OrderBook;
using orderbook::OrderID;
using orderbook::OrderStatus;
using orderbook::Side;
using orderbook::Type;

// Helper function to parse command-line arguments and provide default values.
std::string getCmdOption(char **begin, char **end, const std::string &option, const std::string &default_value = "") {
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end) {
        return *itr;
    }
    return default_value;
}

bool cmdOptionExists(char **begin, char **end, const std::string &option) { return std::find(begin, end, option) != end; }

std::tuple<orderbook::Decimal, orderbook::Decimal, orderbook::Decimal, orderbook::Decimal> getInitialVars(const orderbook::Decimal &lowerBound,
                                                                                                          const orderbook::Decimal &upperBound,
                                                                                                          const orderbook::Decimal &minSpread) {
    orderbook::Decimal bid = (lowerBound + upperBound) / orderbook::Decimal(2, 0);
    orderbook::Decimal ask = bid - minSpread;
    orderbook::Decimal bidQty(10, 0);
    orderbook::Decimal askQty(10, 0);

    return {bid, ask, bidQty, askQty};
}

std::pair<orderbook::Decimal, orderbook::Decimal> getPrice(orderbook::Decimal bid, orderbook::Decimal ask, const orderbook::Decimal &diff, bool dec) {
    if (dec) {
        bid = bid - diff;
        ask = ask - diff;
    } else {
        bid = bid + diff;
        ask = ask + diff;
    }

    return {bid, ask};
}

void latency(int64_t seed, int duration, int pd, orderbook::Decimal lowerBound, orderbook::Decimal upperBound, orderbook::Decimal minSpread, bool sched) {
    std::cout << "Latency function running...\n";
    // Implement the latency benchmark logic.
}

void throughput(int64_t seed, int duration, orderbook::Decimal lowerBound, orderbook::Decimal upperBound, orderbook::Decimal minSpread) {
    std::cout << "starting throughput benchmark...\n";
    // Implement the throughput benchmark logic.
    auto n = orderbook::EmptyNotification();
    auto ob = std::make_unique<orderbook::OrderBook<orderbook::EmptyNotification>>(n);
    orderbook::Decimal bid, ask, bidQty, askQty;
    std::tie(bid, ask, bidQty, askQty) = getInitialVars(lowerBound, upperBound, minSpread);

    uint64_t tok = 0, buyID = 0, sellID = 0, operations = 0;

    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(duration);
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0, 9);

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < end) {
        int r = distribution(generator);
        bool decrease = r < 5;

        std::tie(bid, ask) = getPrice(bid, ask, minSpread, decrease);
        if (bid < lowerBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, false);
        } else if (ask > upperBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, true);
        }

        ob->cancelOrder(++tok, buyID);
        ob->cancelOrder(++tok, sellID);
        buyID = ++tok;
        sellID = ++tok;
        ob->addOrder(buyID, buyID, Type::Limit, Side::Buy, bidQty, bid, Decimal(0, 0), Flag::None);
        ob->addOrder(sellID, sellID, Type::Limit, Side::Sell, askQty, ask, Decimal(0, 0), Flag::None);

        operations += 4;
    }

    auto finish = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::nano> elapsed = finish - start;
    double throughput = operations / (elapsed.count() / 1e9);
    double nanosecPerOp = elapsed.count() / operations;

    std::cout.imbue(std::locale("en_US.UTF-8"));
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total Ops: " << operations << " ops" << std::endl;
    std::cout << "Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "Avg latency: " << nanosecPerOp << " ns/op" << std::endl;
}

void run(int64_t seed, int duration, int pd, const std::string &lb, const std::string &ub, const std::string &ms, const std::string &n, bool sched) {
    orderbook::Decimal lowerBound(lb);
    orderbook::Decimal upperBound(ub);
    orderbook::Decimal minSpread(ms);

    if (n == "latency") {
        latency(seed, duration, pd, lowerBound, upperBound, minSpread, sched);
    } else if (n == "throughput") {
        throughput(seed, duration, lowerBound, upperBound, minSpread);
    }
}

int main(int argc, char *argv[]) {
    int64_t seed = std::stoll(getCmdOption(argv, argv + argc, "-seed", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())));
    int duration = std::stoi(getCmdOption(argv, argv + argc, "-duration", "0"));
    std::string lb = getCmdOption(argv, argv + argc, "-l", "50.0");
    std::string ub = getCmdOption(argv, argv + argc, "-u", "100.0");
    std::string ms = getCmdOption(argv, argv + argc, "-m", "0.25");
    int pd = std::stoi(getCmdOption(argv, argv + argc, "-p", "10"));
    bool sched = cmdOptionExists(argv, argv + argc, "-sched");
    std::string n = getCmdOption(argv, argv + argc, "-n", "latency");

    std::cout << "PID: " << getpid() << std::endl;
    run(seed, duration, pd, lb, ub, ms, n, sched);
    return 0;
}

