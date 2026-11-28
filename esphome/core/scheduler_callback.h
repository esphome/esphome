#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace esphome {

/// Type-erased scheduler callable, built from a callable at the call site: inline when small and trivially
/// copyable, else heap-boxed. A function taking SchedulerCallback && must pass it to the scheduler once, on every path.
class SchedulerCallback {
 public:
  static constexpr size_t INLINE_WORDS = 3;

  template<typename F>
  static constexpr bool FITS_INLINE = sizeof(F) <= INLINE_WORDS * sizeof(uintptr_t) &&
                                      alignof(F) <= alignof(uintptr_t) && std::is_trivially_copyable_v<F>;

  template<typename F>
    requires std::invocable<std::decay_t<F> &>
  SchedulerCallback(F &&f) {
    using D = std::decay_t<F>;
    if constexpr (FITS_INLINE<D>) {
      this->invoke_ = [](SchedulerCallback *self) { (*std::launder(reinterpret_cast<D *>(self->words_)))(); };
      ::new (this->words_) D(std::forward<F>(f));
    } else {
      this->invoke_ = &SchedulerCallback::invoke_boxed;
      this->boxed_.obj = ::new D(std::forward<F>(f));
      this->boxed_.run = [](void *obj) { (*static_cast<D *>(obj))(); };
      if constexpr (std::is_trivially_destructible_v<D> && alignof(D) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        this->boxed_.release = static_cast<void (*)(void *)>(&::operator delete);  // one for all such types
      } else {
        this->boxed_.release = [](void *obj) { ::delete static_cast<D *>(obj); };
      }
    }
  }
  SchedulerCallback(const SchedulerCallback &) = delete;
  SchedulerCallback &operator=(const SchedulerCallback &) = delete;

 private:
  friend class Scheduler;

  SchedulerCallback() : invoke_(nullptr) {}  // a SchedulerItem's slot, filled by take_()

  void run_() { this->invoke_(this); }
  /// Frees a boxed callable; reads nothing after the free, so callers may make it their last step
  void release_() const {
    if (this->invoke_ == &SchedulerCallback::invoke_boxed)
      this->boxed_.release(this->boxed_.obj);
  }
  /// Moves other's callable here and empties other; explicit word stores avoid a memcpy call at -Os
  void take_(SchedulerCallback &other) {
    static_assert(INLINE_WORDS == 3, "take_() copies three words");
    this->invoke_ = other.invoke_;
    this->words_[0] = other.words_[0];
    this->words_[1] = other.words_[1];
    this->words_[2] = other.words_[2];
    other.invoke_ = nullptr;
  }
  static void invoke_boxed(SchedulerCallback *self) { self->boxed_.run(self->boxed_.obj); }

  void (*invoke_)(SchedulerCallback *self);
  union {
    uintptr_t words_[INLINE_WORDS];  // an inline callable's bytes
    struct {
      void *obj;
      void (*run)(void *obj);
      void (*release)(void *obj);
    } boxed_;
  };
};

static_assert(sizeof(SchedulerCallback) == (SchedulerCallback::INLINE_WORDS + 1) * sizeof(uintptr_t));

}  // namespace esphome
