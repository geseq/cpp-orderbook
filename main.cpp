#include <algorithm>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <locale>
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

void throughput(int64_t seed, int duration, int depth, orderbook::Decimal lowerBound, orderbook::Decimal upperBound, orderbook::Decimal minSpread) {
    std::cout << "starting throughput benchmark...\n";
    // Deep-book throughput: maintain a sliding window of `depth` live resting
    // orders with monotonically climbing ids. Each iteration adds one order and,
    // once the book is full, cancels the oldest still-live id — so the book stays
    // ~depth deep and we measure the add/cancel/index/pool path at realistic
    // depth. All orders rest on the bid side (no asks) so nothing ever crosses.
    auto n = orderbook::EmptyNotification();
    auto ob = std::make_unique<orderbook::OrderBook<orderbook::EmptyNotification>>(n);

    orderbook::Decimal qty(10, 0);
    // Spread prices across the band on a minSpread tick grid.
    int64_t ticks = ((upperBound - lowerBound) / minSpread).to_int();
    if (ticks < 1) ticks = 1;

    uint64_t nextID = 0, operations = 0;
    std::deque<OrderID> live;  // ids of resting orders, oldest at front.

    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(duration);
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int64_t> distribution(0, ticks - 1);

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < end) {
        orderbook::Decimal price = lowerBound + minSpread * orderbook::Decimal(distribution(generator), 0);

        OrderID id = ++nextID;
        ob->addOrder(id, Type::Limit, Side::Buy, qty, price, Flag::None);
        live.push_back(id);
        operations += 1;

        if (live.size() > static_cast<size_t>(depth)) {
            ob->cancelOrder(live.front());
            live.pop_front();
            operations += 1;
        }
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

void run(int64_t seed, int duration, int pd, int depth, const std::string &lb, const std::string &ub, const std::string &ms, const std::string &n, bool sched) {
    orderbook::Decimal lowerBound(lb);
    orderbook::Decimal upperBound(ub);
    orderbook::Decimal minSpread(ms);

    if (n == "latency") {
        latency(seed, duration, pd, lowerBound, upperBound, minSpread, sched);
    } else if (n == "throughput") {
        throughput(seed, duration, depth, lowerBound, upperBound, minSpread);
    }
}

int main(int argc, char *argv[]) {
    int64_t seed = std::stoll(getCmdOption(argv, argv + argc, "-seed", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())));
    int duration = std::stoi(getCmdOption(argv, argv + argc, "-duration", "0"));
    std::string lb = getCmdOption(argv, argv + argc, "-l", "50.0");
    std::string ub = getCmdOption(argv, argv + argc, "-u", "100.0");
    std::string ms = getCmdOption(argv, argv + argc, "-m", "0.25");
    int pd = std::stoi(getCmdOption(argv, argv + argc, "-p", "10"));
    int depth = std::stoi(getCmdOption(argv, argv + argc, "-depth", "50000"));
    bool sched = cmdOptionExists(argv, argv + argc, "-sched");
    std::string n = getCmdOption(argv, argv + argc, "-n", "latency");

    std::cout << "PID: " << getpid() << std::endl;
    run(seed, duration, pd, depth, lb, ub, ms, n, sched);
    return 0;
}

