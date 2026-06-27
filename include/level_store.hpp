#pragma once

#include <cstddef>
#include <cstdint>

namespace orderbook {

// Configuration passed uniformly to every LevelStore backend. ArrayLevels reads
// all four fields; RbTreeLevels only uses pool_size and ignores the tick grid
// parameters. Keeping a single config struct lets OrderBook construct either
// backend through the same code path.
struct LevelStoreConfig {
    size_t pool_size = 16384;
    uint64_t base_fp = 0;
    uint64_t tick_fp = 100000000;
    size_t num_ticks = 1 << 16;
};

// LevelStore policy (compile-time, no virtual dispatch). A backend over
// PriceType P must provide:
//
//   explicit Backend(const LevelStoreConfig& cfg);
//   OrderQueue* best();                          // best level, nullptr if empty
//   OrderQueue* findOrCreate(const Decimal& p);  // level at p, creating if absent
//   void        erase(OrderQueue* q);            // drop an emptied level
//   OrderQueue* below(const Decimal& p);         // strictly-lower adjacent level
//   OrderQueue* above(const Decimal& p);         // strictly-higher adjacent level
//   uint64_t    depth() const;                   // number of occupied levels
//
// All methods are header-defined so they inline into PriceLevel. PriceLevel owns
// the shared volume_/num_orders_ accounting and the matching loops; the store
// owns only the price-level container and the OrderQueue pool.

}  // namespace orderbook
