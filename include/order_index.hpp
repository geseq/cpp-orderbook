#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "object_pool.hpp"
#include "order.hpp"
#include "types.hpp"

namespace orderbook {
namespace index {

// Chained hash map over the arbitrary uint64 OrderID -> Order*, tuned for
// 64-bit keys. Each Node {key, val, next} is drawn from a pool::ObjectPool so
// there is no malloc churn; the key lives in the compact node rather than being
// chased through the big Order cacheline on lookup. The bucket array is a power
// of two and the index is a Fibonacci/multiplicative hash
// (key * 2^64/phi) >> (64 - log2buckets), so there is no modulo and the cost is
// independent of id magnitude. Rehash (double + re-link) at load factor ~0.7,
// which is rare given the up-front bucket reserve.
class FibHashIndex {
   public:
    // 2^64 / golden ratio: spreads the high bits of the product across buckets.
    static constexpr uint64_t GOLDEN = 0x9E3779B97F4A7C15ull;

    // Default sizes buckets for a deep live book (~1<<16 entries at lf<=0.7).
    explicit FibHashIndex(size_t reserve_nodes = 1u << 16) : node_pool_(reserve_nodes ? reserve_nodes : 1) {
        // Size buckets so reserve_nodes entries sit at load factor <= ~0.7.
        size_t want = reserve_nodes + reserve_nodes / 2 + 1;
        size_t cap = 16;
        while (cap < want) cap <<= 1;
        setCapacity(cap);
    }

    FibHashIndex(const FibHashIndex&) = delete;
    FibHashIndex& operator=(const FibHashIndex&) = delete;

    Order* find(uint64_t id) const {
        for (Node* n = buckets_[bucketOf(id)]; n != nullptr; n = n->next) {
            if (n->key == id) return n->val;
        }
        return nullptr;
    }

    bool contains(uint64_t id) const { return find(id) != nullptr; }

    void insert(uint64_t id, Order* o) {
        // Caller must dedup (contains()) first; insert never overwrites a live id.
        assert(find(id) == nullptr && "FibHashIndex::insert on existing id");
        if (size_ + 1 > grow_threshold_) rehash(cap_ << 1);
        size_t b = bucketOf(id);
        Node* n = node_pool_.acquire();
        n->key = id;
        n->val = o;
        n->next = buckets_[b];
        buckets_[b] = n;
        ++size_;
    }

    // Unlink the node for id, recycle it to the pool, and return its Order*
    // (nullptr if absent).
    Order* erase(uint64_t id) {
        size_t b = bucketOf(id);
        Node* prev = nullptr;
        for (Node* n = buckets_[b]; n != nullptr; prev = n, n = n->next) {
            if (n->key == id) {
                if (prev == nullptr) {
                    buckets_[b] = n->next;
                } else {
                    prev->next = n->next;
                }
                Order* v = n->val;
                node_pool_.release(n);
                --size_;
                return v;
            }
        }
        return nullptr;
    }

   private:
    struct Node {
        uint64_t key;
        Order* val;
        Node* next;
    };

    size_t bucketOf(uint64_t id) const { return static_cast<size_t>((id * GOLDEN) >> shift_); }

    void setCapacity(size_t cap) {
        cap_ = cap;
        unsigned log2cap = 0;
        while ((size_t(1) << log2cap) < cap) ++log2cap;
        shift_ = 64u - log2cap;  // top log2cap bits of the product select a bucket
        buckets_.assign(cap, nullptr);
        grow_threshold_ = (cap * 7) / 10;  // load factor ~0.7
    }

    // Double the bucket array and re-link every node (nodes themselves are kept).
    void rehash(size_t new_cap) {
#ifdef FIBHASH_COUNT_REHASH
        ++rehash_count_;
#endif
        std::vector<Node*> old = std::move(buckets_);
        setCapacity(new_cap);
        for (Node* head : old) {
            while (head != nullptr) {
                Node* next = head->next;
                size_t b = bucketOf(head->key);
                head->next = buckets_[b];
                buckets_[b] = head;
                head = next;
            }
        }
    }

    std::vector<Node*> buckets_;
    size_t cap_ = 0;
    unsigned shift_ = 0;
    size_t size_ = 0;
    size_t grow_threshold_ = 0;
    pool::ObjectPool<Node> node_pool_;
#ifdef FIBHASH_COUNT_REHASH
   public:
    size_t rehash_count_ = 0;  // test-only: confirms the rehash path is exercised
#endif
};

}  // namespace index
}  // namespace orderbook
