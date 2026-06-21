#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace pool {

// O(1)-acquire / O(1)-release slab allocator with an intrusive LIFO free list.
// Slabs double on growth and live objects are never relocated. Not thread-safe.
// Callers must release everything they acquire; the dtor frees slab memory but
// does not run destructors on outstanding objects.
template <typename T>
class ObjectPool {
   public:
    explicit ObjectPool(size_t fixed_size) {
        next_slab_size_ = fixed_size ? fixed_size : 1;
        allocate_slab(next_slab_size_);
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ~ObjectPool() {
        for (Slot* slab : slabs_) {
            ::operator delete[](static_cast<void*>(slab), std::align_val_t{alignof(T)});
        }
    }

    template <typename... Args>
    T* acquire(Args&&... args) {
        if (free_head_ == nullptr) {
            allocate_slab(next_slab_size_);
        }
        Slot* slot = free_head_;
        free_head_ = slot->next;
        return new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
    }

    bool release(T* obj) {
        obj->~T();
        Slot* slot = reinterpret_cast<Slot*>(obj);
        slot->next = free_head_;
        free_head_ = slot;
        return true;
    }

   private:
    // A free slot stores its free-list link in its own storage, so there is no
    // per-object overhead.
    union Slot {
        Slot* next;
        std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    };

    void allocate_slab(size_t count) {
        // Reserve the slot first so push_back can't throw after the raw alloc.
        slabs_.reserve(slabs_.size() + 1);
        Slot* slab = static_cast<Slot*>(
            ::operator new[](count * sizeof(Slot), std::align_val_t{alignof(T)}));
        slabs_.push_back(slab);
        for (size_t i = 0; i < count; ++i) {
            slab[i].next = free_head_;
            free_head_ = &slab[i];
        }
        next_slab_size_ = count * 2;
    }

    Slot* free_head_ = nullptr;
    size_t next_slab_size_ = 0;
    std::vector<Slot*> slabs_;
};

}  // namespace pool
