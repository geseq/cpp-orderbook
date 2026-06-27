#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include "boost/intrusive/rbtree.hpp"
#include "level_store.hpp"
#include "object_pool.hpp"
#include "orderqueue.hpp"
#include "types.hpp"

namespace orderbook {

using CmpGreater = boost::intrusive::compare<std::greater<>>;
using CmpLess = boost::intrusive::compare<std::less<>>;

// LevelStore backend: the original boost::intrusive::rbtree<OrderQueue>.
//
// Bids are ordered highest-first (CmpGreater), asks lowest-first (CmpLess), so
// begin() is always the best level. Neighbour queries use lower_bound /
// upper_bound with PriceCompare. The tick grid parameters in LevelStoreConfig
// are ignored; only pool_size is used.
template <PriceType P>
class RbTreeLevels {
    pool::ObjectPool<OrderQueue> queue_pool_;

    using CompareType = std::conditional_t<P == PriceType::Bid, CmpGreater, CmpLess>;
    using PriceTree = boost::intrusive::rbtree<OrderQueue, CompareType>;
    PriceTree price_tree_;

    uint64_t depth_ = 0;

    // Heterogeneous OrderQueue/Decimal comparator consistent with the tree's
    // value ordering (Bid: descending, Ask: ascending). Used for the bound
    // queries below so neighbour lookups stay correct on both sides.
    struct KeyCmp {
        bool operator()(const OrderQueue& q, const Decimal& p) const {
            if constexpr (P == PriceType::Bid) {
                return q.price() > p;
            } else {
                return q.price() < p;
            }
        }
        bool operator()(const Decimal& p, const OrderQueue& q) const {
            if constexpr (P == PriceType::Bid) {
                return p > q.price();
            } else {
                return p < q.price();
            }
        }
    };

   public:
    explicit RbTreeLevels(const LevelStoreConfig& cfg) : queue_pool_(cfg.pool_size) {}

    [[nodiscard]] OrderQueue* best() {
        auto it = price_tree_.begin();
        if (it != price_tree_.end()) {
            return &*it;
        }
        return nullptr;
    }

    [[nodiscard]] OrderQueue* findOrCreate(const Decimal& price) {
        auto it = price_tree_.find(price);
        if (it == price_tree_.end()) {
            OrderQueue* q = queue_pool_.acquire(price);
            price_tree_.insert_equal(*q);
            ++depth_;
            return q;
        }
        return &*it;
    }

    void erase(OrderQueue* q) {
        price_tree_.erase(price_tree_.iterator_to(*q));
        --depth_;
        queue_pool_.release(q);
    }

    // Highest occupied level whose price is strictly < the query price.
    [[nodiscard]] OrderQueue* below(const Decimal& price) {
        if constexpr (P == PriceType::Bid) {
            // Descending tree: first element strictly below the query is the
            // highest such price.
            auto it = price_tree_.upper_bound(price, KeyCmp());
            if (it != price_tree_.end()) {
                return &*it;
            }
            return nullptr;
        } else {
            // Ascending tree: the element just before lower_bound is the
            // highest price strictly below the query.
            auto it = price_tree_.lower_bound(price, KeyCmp());
            if (it != price_tree_.begin()) {
                --it;
                return &*it;
            }
            return nullptr;
        }
    }

    // Lowest occupied level whose price is strictly > the query price.
    [[nodiscard]] OrderQueue* above(const Decimal& price) {
        if constexpr (P == PriceType::Bid) {
            // Descending tree: the element just before lower_bound is the
            // lowest price strictly above the query.
            auto it = price_tree_.lower_bound(price, KeyCmp());
            if (it != price_tree_.begin()) {
                --it;
                return &*it;
            }
            return nullptr;
        } else {
            // Ascending tree: first element strictly above the query is the
            // lowest such price.
            auto it = price_tree_.upper_bound(price, KeyCmp());
            if (it != price_tree_.end()) {
                return &*it;
            }
            return nullptr;
        }
    }

    [[nodiscard]] uint64_t depth() const { return depth_; }
};

}  // namespace orderbook
