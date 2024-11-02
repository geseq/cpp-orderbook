#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <type_traits>
#include <vector>

#include "include/orderbook.hpp"
#include "include/types.hpp"

using namespace orderbook;

constexpr int64_t ITERATIONS_PER_SECOND = 2'000'000;

// TscDurationClock implementation
class TscDurationClock {
   public:
    TscDurationClock() = delete;
    static uint64_t tsc() {
        uint64_t x;
        asm volatile(
            "mfence\n"
            "lfence\n"
            "rdtsc\n"
            "shl $32, %%rdx\n"
            "or %%rdx, %%rax"
            : "=a"(x)
            :
            : "rdx");
        return x;
    }
    static std::chrono::nanoseconds duration_since_tsc(uint64_t start) {
        uint64_t end = bench_end();
        uint64_t tsc_diff = end - start;
        return std::chrono::nanoseconds{static_cast<uint64_t>((static_cast<double>(tsc_diff) / tsc_frequency) * 1e9)};
    }
    static void calibrate_frequency() {
        using namespace std::chrono;
        auto start = steady_clock::now();
        uint64_t start_tsc = tsc();
        std::this_thread::sleep_for(seconds(1));
        auto end = steady_clock::now();
        uint64_t end_tsc = bench_end();
        duration<double> elapsed_seconds = end - start;
        tsc_frequency = static_cast<double>(end_tsc - start_tsc) / elapsed_seconds.count();
    }

   private:
    static uint64_t bench_end() {
        uint64_t x;
        asm volatile(
            "rdtscp\n"
            "lfence\n"
            "shl $32, %%rdx\n"
            "or %%rdx, %%rax"
            : "=a"(x)
            :
            : "rdx", "rcx");
        return x;
    }
    static double tsc_frequency;
};

double TscDurationClock::tsc_frequency = 0.0;

// Clock selection template
template <bool UseTsc>
struct ClockSelector {
    using Clock = std::chrono::steady_clock;
    static auto now() { return Clock::now(); }
    static auto duration(Clock::time_point start, Clock::time_point end) { return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start); }
};

template <>
struct ClockSelector<true> {
    using Clock = TscDurationClock;
    static auto now() { return Clock::tsc(); }
    static auto duration(uint64_t start, uint64_t end) { return Clock::duration_since_tsc(start); }
};

// Define which clock to use (set to true to use TscDurationClock, false for steady_clock)
constexpr bool USE_TSC_CLOCK = true;
using SelectedClock = ClockSelector<USE_TSC_CLOCK>;

class Histogram {
   private:
    std::vector<uint64_t> buckets;
    double min_value;
    double max_value;
    double bucket_width;
    uint64_t total_count;

   public:
    Histogram(double min, double max, size_t bucket_count)
        : buckets(bucket_count, 0), min_value(min), max_value(max), bucket_width((max - min) / bucket_count), total_count(0) {}

    void record(double value) {
        if (value < min_value) {
            ++buckets[0];
        } else if (value >= max_value) {
            ++buckets[buckets.size() - 1];
        } else {
            size_t bucket = static_cast<size_t>((value - min_value) / bucket_width);
            ++buckets[bucket];
        }
        ++total_count;
    }

    double percentile(double p) const {
        if (total_count == 0) return 0.0;

        uint64_t count = static_cast<uint64_t>(std::ceil(p / 100.0 * total_count));
        uint64_t cumulative = 0;

        for (size_t i = 0; i < buckets.size(); ++i) {
            cumulative += buckets[i];
            if (cumulative >= count) {
                double bucket_start = min_value + i * bucket_width;
                double bucket_end = bucket_start + bucket_width;
                double fraction = static_cast<double>(cumulative - count) / buckets[i];
                return bucket_end - fraction * bucket_width;
            }
        }

        return max_value;
    }
};

std::tuple<Decimal, Decimal, Decimal, Decimal> getInitialVars(const Decimal &lowerBound, const Decimal &upperBound, const Decimal &minSpread) {
    Decimal bid = (lowerBound + upperBound) / Decimal(2, 0);
    Decimal ask = bid - minSpread;
    Decimal bidQty(10, 0);
    Decimal askQty(10, 0);
    return {bid, ask, bidQty, askQty};
}

std::pair<Decimal, Decimal> getPrice(Decimal bid, Decimal ask, const Decimal &diff, bool dec) {
    if (dec) {
        bid = bid - diff;
        ask = ask - diff;
    } else {
        bid = bid + diff;
        ask = ask + diff;
    }
    return {bid, ask};
}

void printResultsWithPercentiles(const std::string &operationName, const Histogram &hist) {
    std::vector<double> percentiles = {50, 75, 90, 95, 99, 99.9, 99.99, 99.9999, 100};
    std::cout << "Operation: " << operationName << std::endl;
    for (double p : percentiles) {
        std::cout << std::fixed << std::setprecision(6) << p << ": " << hist.percentile(p) << " ns" << std::endl;
    }
}

std::pair<Histogram, Histogram> runBenchmarkLatency(int duration, const Decimal &lowerBound, const Decimal &upperBound, const Decimal &minSpread) {
    int64_t seed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto n = EmptyNotification();
    auto ob = std::make_unique<OrderBook<EmptyNotification>>(n);

    Decimal bid, ask, bidQty, askQty;
    std::tie(bid, ask, bidQty, askQty) = getInitialVars(lowerBound, upperBound, minSpread);

    uint64_t tok = 0, buyID = 0, sellID = 0;
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 9);

    Histogram addHist(10.0, 1000000.0, 10000);     // 1ns to 10µs, 1000 buckets
    Histogram cancelHist(10.0, 1000000.0, 10000);  // 1ns to 10µs, 1000 buckets

    uint64_t iterations = static_cast<uint64_t>(duration) * ITERATIONS_PER_SECOND;

    uint64_t warmup = ITERATIONS_PER_SECOND;
    for (uint64_t i = 0; i < warmup; ++i) {
        bool dec = dis(gen) < 5;

        std::tie(bid, ask) = getPrice(bid, ask, minSpread, dec);
        if (bid < lowerBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, false);
        } else if (bid > upperBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, true);
        }

        ob->cancelOrder(++tok, buyID);
        ob->cancelOrder(++tok, sellID);
        buyID = ++tok;
        sellID = ++tok;

        ob->addOrder(buyID, buyID, Type::Limit, Side::Buy, bidQty, bid, Decimal(0, 0), Flag::None);
        ob->addOrder(sellID, sellID, Type::Limit, Side::Sell, askQty, ask, Decimal(0, 0), Flag::None);
    }

    auto loopStart = SelectedClock::now();
    for (uint64_t i = 0; i < iterations; ++i) {
        bool dec = dis(gen) < 5;

        std::tie(bid, ask) = getPrice(bid, ask, minSpread, dec);
        if (bid < lowerBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, false);
        } else if (bid > upperBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, true);
        }

        auto start = SelectedClock::now();
        ob->cancelOrder(++tok, buyID);
        cancelHist.record(SelectedClock::duration(start, SelectedClock::now()).count());

        start = SelectedClock::now();
        ob->cancelOrder(++tok, sellID);
        cancelHist.record(SelectedClock::duration(start, SelectedClock::now()).count());

        buyID = ++tok;
        sellID = ++tok;

        start = SelectedClock::now();
        ob->addOrder(buyID, buyID, Type::Limit, Side::Buy, bidQty, bid, Decimal(0, 0), Flag::None);
        addHist.record(SelectedClock::duration(start, SelectedClock::now()).count());

        start = SelectedClock::now();
        ob->addOrder(sellID, sellID, Type::Limit, Side::Sell, askQty, ask, Decimal(0, 0), Flag::None);
        addHist.record(SelectedClock::duration(start, SelectedClock::now()).count());
    }

    uint64_t operations = iterations * 4;
    auto elapsed = SelectedClock::duration(loopStart, SelectedClock::now());
    double throughput = static_cast<double>(operations) / (elapsed.count() / 1e9);
    double avgLatency = static_cast<double>(elapsed.count()) / operations;

    std::cout << std::fixed << std::setprecision(0) << "Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << std::fixed << std::setprecision(2) << "Avg latency: " << avgLatency << " ns/op" << std::endl;

    return {addHist, cancelHist};
}

std::pair<double, double> runBenchmarkThroughput(int duration, const Decimal &lowerBound, const Decimal &upperBound, const Decimal &minSpread) {
    int64_t seed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto n = EmptyNotification();
    auto ob = std::make_unique<OrderBook<EmptyNotification>>(n);

    Decimal bid, ask, bidQty, askQty;
    std::tie(bid, ask, bidQty, askQty) = getInitialVars(lowerBound, upperBound, minSpread);

    uint64_t tok = 0, buyID = 0, sellID = 0;
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 9);

    uint64_t iterations = static_cast<uint64_t>(duration) * ITERATIONS_PER_SECOND;

    auto start = SelectedClock::now();
    for (uint64_t i = 0; i < iterations; ++i) {
        bool dec = dis(gen) < 5;

        std::tie(bid, ask) = getPrice(bid, ask, minSpread, dec);
        if (bid < lowerBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, false);
        } else if (bid > upperBound) {
            std::tie(bid, ask) = getPrice(bid, ask, minSpread, true);
        }

        ob->cancelOrder(++tok, buyID);
        ob->cancelOrder(++tok, sellID);
        buyID = ++tok;
        sellID = ++tok;

        ob->addOrder(buyID, buyID, Type::Limit, Side::Buy, bidQty, bid, Decimal(0, 0), Flag::None);
        ob->addOrder(sellID, sellID, Type::Limit, Side::Sell, askQty, ask, Decimal(0, 0), Flag::None);
    }

    uint64_t operations = iterations * 4;
    auto elapsed = SelectedClock::duration(start, SelectedClock::now());
    double throughput = static_cast<double>(operations) / (elapsed.count() / 1e9);
    double avgLatency = static_cast<double>(elapsed.count()) / operations;

    return {throughput, avgLatency};
}

int main(int argc, char *argv[]) {
    if constexpr (USE_TSC_CLOCK) {
        TscDurationClock::calibrate_frequency();
    }

    Decimal lowerBound("50.0");
    Decimal upperBound("100.0");
    Decimal minSpread("0.25");
    int duration = 30;  // seconds

    std::cout << "Running Latency Benchmark..." << std::endl;
    auto [addHist, cancelHist] = runBenchmarkLatency(duration, lowerBound, upperBound, minSpread);
    printResultsWithPercentiles("AddOrder", addHist);
    printResultsWithPercentiles("CancelOrder", cancelHist);

    std::cout << "\nRunning Throughput Benchmark..." << std::endl;
    auto [throughput, avgLatency] = runBenchmarkThroughput(duration, lowerBound, upperBound, minSpread);
    std::cout << std::fixed << std::setprecision(0) << "Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << std::fixed << std::setprecision(2) << "Avg latency: " << avgLatency << " ns/op" << std::endl;

    return 0;
}
