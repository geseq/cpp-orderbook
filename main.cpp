#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "include/orderbook.hpp"
#include "include/types.hpp"

// Helper function to parse command-line arguments and provide default values.
std::string getCmdOption(char **begin, char **end, const std::string &option, const std::string &default_value = "") {
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end) {
        return *itr;
    }
    return default_value;
}

bool cmdOptionExists(char **begin, char **end, const std::string &option) { return std::find(begin, end, option) != end; }

void latency(int64_t seed, int duration, int pd, orderbook::Decimal lowerBound, orderbook::Decimal upperBound, orderbook::Decimal minSpread, bool sched) {
    std::cout << "Latency function running...\n";
    // Implement the latency benchmark logic.
}

void throughput(int64_t seed, int duration, orderbook::Decimal lowerBound, orderbook::Decimal upperBound, orderbook::Decimal minSpread) {
    std::cout << "Throughput function running...\n";
    // Implement the throughput benchmark logic.
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

