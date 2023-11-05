#pragma once

#include <functional>

class scope_exit {
   public:
    scope_exit(std::function<void()> f) : f_(std::move(f)) {}

    ~scope_exit() {
        if (active_) {
            f_();
        }
    }

    void release() { active_ = false; }

   private:
    std::function<void()> f_;
    bool active_ = true;
};
