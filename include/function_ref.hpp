#pragma once

#include <type_traits>
#include <utility>

namespace orderbook {

// Non-owning view over any callable, modeled after std::function_ref (P0792).
// Stores a type-erased object pointer plus a thunk; trivially copyable, two
// pointers wide, performs no allocation. The referenced callable must outlive
// every invocation through the function_ref.
template <class Signature>
class function_ref;

template <class R, class... Args>
class function_ref<R(Args...)> {
    void* obj_ = nullptr;
    R (*thunk_)(void*, Args...) = nullptr;

   public:
    template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function_ref>>>
    function_ref(F&& f) noexcept
        : obj_(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),
          thunk_([](void* obj, Args... args) -> R {
              return (*static_cast<std::remove_reference_t<F>*>(obj))(std::forward<Args>(args)...);
          }) {}

    R operator()(Args... args) const { return thunk_(obj_, std::forward<Args>(args)...); }
};

}  // namespace orderbook
