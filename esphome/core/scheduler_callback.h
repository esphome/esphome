#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "esphome/core/defines.h"

namespace esphome {

/// Type-erased scheduler callable, built from a callable at the call site: inline when small and trivially
/// copyable, else heap-boxed. A function taking SchedulerCallback && must pass it to the scheduler once, on every
/// path: one that is dropped leaks its box and what it captured. A boxed capture's destructor runs after the
/// scheduler lock is released, so it may call the scheduler.
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
      this->invoke_ = [](SchedulerCallback *self) { (*std::launder(reinterpret_cast<D *>(self->bytes_)))(); };
      ::new (this->bytes_) D(std::forward<F>(f));
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
#ifdef ESPHOME_DEBUG_SCHEDULER
  // Catches one dropped before reaching the scheduler, at the call site (also with NDEBUG)
  ~SchedulerCallback() {
    if (this->invoke_ != nullptr)
      report_dropped();
  }
#endif

 private:
  friend class Scheduler;

  SchedulerCallback() : invoke_(nullptr) {}  // a SchedulerItem's slot, filled by take_()

  void run_() { this->invoke_(this); }
  /// Only a boxed callable has a destructor to run; an inline one is trivially copyable
  bool is_boxed_() const { return this->invoke_ == &SchedulerCallback::invoke_boxed; }
  /// Frees a boxed callable
  void release_() {
    const bool boxed = this->is_boxed_();
#ifdef ESPHOME_DEBUG_SCHEDULER
    this->invoke_ = nullptr;  // empty for the destructor's check
#endif
    if (boxed)
      this->boxed_.release(this->boxed_.obj);
  }
  /// Moves other's callable here and empties other. Copied as bytes (relying on GCC/Clang union and memcpy
  /// semantics) in word-sized pieces, which -Os keeps as word loads where one 12-byte copy can become a memcpy call
  void take_(SchedulerCallback &other) {
    this->invoke_ = other.invoke_;
    auto *dst = static_cast<unsigned char *>(__builtin_assume_aligned(this->bytes_, alignof(uintptr_t)));
    const auto *src = static_cast<const unsigned char *>(__builtin_assume_aligned(other.bytes_, alignof(uintptr_t)));
    for (size_t i = 0; i < sizeof(this->bytes_); i += sizeof(uintptr_t))
      __builtin_memcpy(dst + i, src + i, sizeof(uintptr_t));
    other.invoke_ = nullptr;
  }
#ifdef ESPHOME_DEBUG_SCHEDULER
  [[noreturn]] static void report_dropped();  // defined in scheduler.cpp
#endif
  static void invoke_boxed(SchedulerCallback *self) { self->boxed_.run(self->boxed_.obj); }

  void (*invoke_)(SchedulerCallback *self);
  union {
    alignas(uintptr_t) unsigned char bytes_[INLINE_WORDS * sizeof(uintptr_t)];  // an inline callable
    struct {
      void *obj;
      void (*run)(void *obj);
      void (*release)(void *obj);
    } boxed_;
  };
};

static_assert(sizeof(SchedulerCallback) == (SchedulerCallback::INLINE_WORDS + 1) * sizeof(uintptr_t));

}  // namespace esphome
