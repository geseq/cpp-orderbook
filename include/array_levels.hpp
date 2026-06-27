#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "level_store.hpp"
#include "object_pool.hpp"
#include "orderqueue.hpp"
#include "types.hpp"

namespace orderbook {

namespace detail {
// Round x up to the next multiple of 64 and divide by 64 (number of words).
inline size_t words_for(size_t bits) { return (bits + 63) / 64; }
}  // namespace detail

// LevelStore backend: tick-indexed array + 3-level occupancy bitmap.
//
// Each price maps to a fixed slot index = (price.fp - base_fp) / tick_fp.
// levels_[slot] is the OrderQueue at that price (nullptr == empty). A 3-level
// uint64 bitmap tracks which slots are occupied so best-price / neighbour
// queries are O(1)-ish via hardware bit-scan instead of an O(log N) tree walk.
//
// The slot array + bitmap GROW on demand: a price whose tick index lands beyond
// the current capacity triggers grow(), which enlarges levels_ and the bitmap
// (capacity is doubled / rounded up to a multiple of 64 so bitmap words stay
// aligned). Growth is amortized O(1) and rare with good initial sizing.
template <PriceType P>
class ArrayLevels {
    pool::ObjectPool<OrderQueue> queue_pool_;

    uint64_t base_fp_ = 0;
    uint64_t tick_fp_ = 1;
    size_t num_ticks_ = 0;

    // Slot array: levels_[tickIndex(price)] -> OrderQueue* (nullptr == empty).
    std::vector<OrderQueue*> levels_;

    // Hierarchical occupancy bitmap over the num_ticks_ slots.
    std::vector<uint64_t> l2_;
    std::vector<uint64_t> l1_;
    std::vector<uint64_t> l0_;

    uint64_t depth_ = 0;

    // --- tick math -----------------------------------------------------------
    [[nodiscard]] size_t tickIndex(const Decimal& price) const {
        const uint64_t fp = price.fp;
        // Cheap debug-only sanity checks. No upper-bound check: indices above the
        // current capacity are handled by grow() rather than crashing.
        assert(fp >= base_fp_ && "price below tick grid base");
        const uint64_t delta = fp - base_fp_;
        assert(delta % tick_fp_ == 0 && "price not tick-aligned");
        return static_cast<size_t>(delta / tick_fp_);
    }

    // Enlarge levels_ and the 3-level bitmap so that tick `needed_index` is in
    // range. Amortized O(1).
    void grow(size_t needed_index) {
        size_t new_cap = std::max(needed_index + 1, levels_.size() * 2);
        new_cap = (new_cap + 63) & ~static_cast<size_t>(63);  // round up to multiple of 64

        levels_.resize(new_cap, nullptr);
        num_ticks_ = new_cap;

        const size_t l2w = detail::words_for(new_cap);
        const size_t l1w = detail::words_for(l2w);
        const size_t l0w = detail::words_for(l1w);
        if (l2_.size() < l2w) {
            l2_.resize(l2w, 0);
        }
        if (l1_.size() < l1w) {
            l1_.resize(l1w, 0);
        }
        if (l0_.size() < l0w) {
            l0_.resize(l0w, 0);
        }
    }

    // --- bitmap primitives ---------------------------------------------------
    void setBit(size_t t) {
        const size_t w = t >> 6;
        const uint64_t b = uint64_t{1} << (t & 63);
        if (l2_[w] & b) {
            return;
        }
        const bool l2_was_zero = (l2_[w] == 0);
        l2_[w] |= b;
        if (!l2_was_zero) {
            return;
        }
        const size_t w1 = w >> 6;
        const uint64_t b1 = uint64_t{1} << (w & 63);
        const bool l1_was_zero = (l1_[w1] == 0);
        l1_[w1] |= b1;
        if (!l1_was_zero) {
            return;
        }
        const size_t w0 = w1 >> 6;
        const uint64_t b0 = uint64_t{1} << (w1 & 63);
        l0_[w0] |= b0;
    }

    void clearBit(size_t t) {
        const size_t w = t >> 6;
        const uint64_t b = uint64_t{1} << (t & 63);
        l2_[w] &= ~b;
        if (l2_[w] != 0) {
            return;
        }
        const size_t w1 = w >> 6;
        const uint64_t b1 = uint64_t{1} << (w & 63);
        l1_[w1] &= ~b1;
        if (l1_[w1] != 0) {
            return;
        }
        const size_t w0 = w1 >> 6;
        const uint64_t b0 = uint64_t{1} << (w1 & 63);
        l0_[w0] &= ~b0;
    }

    // Highest occupied tick, or -1 if the bitmap is empty.
    [[nodiscard]] int highestSet() const { return highestSetBelow(static_cast<int>(num_ticks_)); }
    // Lowest occupied tick, or -1 if the bitmap is empty.
    [[nodiscard]] int lowestSet() const { return lowestSetAbove(-1); }

    // Highest occupied tick strictly below t, or -1.
    [[nodiscard]] int highestSetBelow(int t) const {
        if (t <= 0) {
            return -1;
        }
        if (t > static_cast<int>(num_ticks_)) {
            t = static_cast<int>(num_ticks_);
        }
        const int idx = t - 1;  // highest candidate slot
        const size_t w = static_cast<size_t>(idx) >> 6;
        const int bit = idx & 63;

        // Mask to bits <= bit within the leaf word containing idx.
        const uint64_t mask = (bit == 63) ? ~uint64_t{0} : ((uint64_t{1} << (bit + 1)) - 1);
        uint64_t word = l2_[w] & mask;
        if (word) {
            return static_cast<int>(w * 64 + (63 - __builtin_clzll(word)));
        }

        // No hit in this leaf word; find the highest nonzero leaf word with index < w.
        const size_t w1 = w >> 6;
        const int bit1 = static_cast<int>(w & 63);
        const uint64_t mask1 = (bit1 == 0) ? uint64_t{0} : ((uint64_t{1} << bit1) - 1);
        uint64_t word1 = l1_[w1] & mask1;
        if (word1) {
            const size_t lw = w1 * 64 + (63 - __builtin_clzll(word1));
            return static_cast<int>(lw * 64 + (63 - __builtin_clzll(l2_[lw])));
        }

        // Walk up to the L0 summary to find the highest nonzero L1 word index < w1.
        const size_t w0 = w1 >> 6;
        const int bit0 = static_cast<int>(w1 & 63);
        const uint64_t mask0 = (bit0 == 0) ? uint64_t{0} : ((uint64_t{1} << bit0) - 1);
        uint64_t word0 = l0_[w0] & mask0;
        size_t target_l1;
        if (word0) {
            target_l1 = w0 * 64 + (63 - __builtin_clzll(word0));
        } else {
            int found = -1;
            for (int i = static_cast<int>(w0) - 1; i >= 0; --i) {
                if (l0_[i]) {
                    found = i * 64 + (63 - __builtin_clzll(l0_[i]));
                    break;
                }
            }
            if (found < 0) {
                return -1;
            }
            target_l1 = static_cast<size_t>(found);
        }
        const size_t lw = target_l1 * 64 + (63 - __builtin_clzll(l1_[target_l1]));
        return static_cast<int>(lw * 64 + (63 - __builtin_clzll(l2_[lw])));
    }

    // Lowest occupied tick strictly above t, or -1.
    [[nodiscard]] int lowestSetAbove(int t) const {
        int idx = t + 1;  // lowest candidate slot
        if (idx < 0) {
            idx = 0;
        }
        if (idx >= static_cast<int>(num_ticks_)) {
            return -1;
        }
        const size_t w = static_cast<size_t>(idx) >> 6;
        const int bit = idx & 63;

        // Mask to bits >= bit within the leaf word containing idx.
        const uint64_t mask = ~uint64_t{0} << bit;
        uint64_t word = l2_[w] & mask;
        if (word) {
            return static_cast<int>(w * 64 + __builtin_ctzll(word));
        }

        // Find the lowest nonzero leaf word with index > w.
        const size_t w1 = w >> 6;
        const int bit1 = static_cast<int>(w & 63);
        const uint64_t mask1 = (bit1 == 63) ? uint64_t{0} : (~uint64_t{0} << (bit1 + 1));
        uint64_t word1 = l1_[w1] & mask1;
        if (word1) {
            const size_t lw = w1 * 64 + __builtin_ctzll(word1);
            return static_cast<int>(lw * 64 + __builtin_ctzll(l2_[lw]));
        }

        // Walk up to the L0 summary to find the lowest nonzero L1 word index > w1.
        const size_t w0 = w1 >> 6;
        const int bit0 = static_cast<int>(w1 & 63);
        const uint64_t mask0 = (bit0 == 63) ? uint64_t{0} : (~uint64_t{0} << (bit0 + 1));
        uint64_t word0 = l0_[w0] & mask0;
        size_t target_l1;
        if (word0) {
            target_l1 = w0 * 64 + __builtin_ctzll(word0);
        } else {
            int found = -1;
            for (size_t i = w0 + 1; i < l0_.size(); ++i) {
                if (l0_[i]) {
                    found = static_cast<int>(i * 64 + __builtin_ctzll(l0_[i]));
                    break;
                }
            }
            if (found < 0) {
                return -1;
            }
            target_l1 = static_cast<size_t>(found);
        }
        const size_t lw = target_l1 * 64 + __builtin_ctzll(l1_[target_l1]);
        return static_cast<int>(lw * 64 + __builtin_ctzll(l2_[lw]));
    }

   public:
    explicit ArrayLevels(const LevelStoreConfig& cfg)
        : queue_pool_(cfg.pool_size), base_fp_(cfg.base_fp), tick_fp_(cfg.tick_fp), num_ticks_(cfg.num_ticks) {
        assert(tick_fp_ != 0 && "tick_fp must be non-zero");
        assert(num_ticks_ <= (size_t{64} * 64 * 64) && "num_ticks exceeds 3-level bitmap capacity");

        levels_.assign(num_ticks_, nullptr);

        const size_t l2w = detail::words_for(num_ticks_);
        const size_t l1w = detail::words_for(l2w);
        const size_t l0w = detail::words_for(l1w);
        l2_.assign(l2w, 0);
        l1_.assign(l1w, 0);
        l0_.assign(l0w, 0);
    }

    [[nodiscard]] OrderQueue* best() {
        int t;
        if constexpr (P == PriceType::Bid) {
            t = highestSet();
        } else {
            t = lowestSet();
        }
        if (t < 0) {
            return nullptr;
        }
        return levels_[static_cast<size_t>(t)];
    }

    [[nodiscard]] OrderQueue* findOrCreate(const Decimal& price) {
        const size_t t = tickIndex(price);
        if (t >= levels_.size()) {
            grow(t);
        }
        OrderQueue* q = levels_[t];
        if (q == nullptr) {
            q = queue_pool_.acquire(price);
            levels_[t] = q;
            setBit(t);
            ++depth_;
        }
        return q;
    }

    void erase(OrderQueue* q) {
        const size_t t = tickIndex(q->price());
        clearBit(t);
        queue_pool_.release(q);
        levels_[t] = nullptr;
        --depth_;
    }

    // Highest occupied level whose price is strictly < the query price.
    [[nodiscard]] OrderQueue* below(const Decimal& price) {
        // price(t) < query  <=>  t * tick_fp < (price.fp - base_fp)
        // so the lowest tick at or above the query is ceil((fp - base)/tick).
        const uint64_t fp = price.fp;
        if (fp <= base_fp_) {
            return nullptr;
        }
        const uint64_t delta = fp - base_fp_;
        const uint64_t ceil_idx = (delta + tick_fp_ - 1) / tick_fp_;  // first tick with price >= query
        const int t = highestSetBelow(static_cast<int>(ceil_idx));
        if (t < 0) {
            return nullptr;
        }
        return levels_[static_cast<size_t>(t)];
    }

    // Lowest occupied level whose price is strictly > the query price.
    [[nodiscard]] OrderQueue* above(const Decimal& price) {
        // price(t) > query  <=>  t * tick_fp > (price.fp - base_fp)
        // so the last tick at or below the query is floor((fp - base)/tick).
        const uint64_t fp = price.fp;
        const uint64_t delta = (fp >= base_fp_) ? (fp - base_fp_) : 0;
        const uint64_t floor_idx = delta / tick_fp_;  // last tick with price <= query
        const int t = lowestSetAbove(static_cast<int>(floor_idx));
        if (t < 0) {
            return nullptr;
        }
        return levels_[static_cast<size_t>(t)];
    }

    [[nodiscard]] uint64_t depth() const { return depth_; }
};

}  // namespace orderbook
