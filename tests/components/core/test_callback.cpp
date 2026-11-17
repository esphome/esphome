#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

using IntCallback = Callback<void(int)>;

// Counts destructions of the copies a callable carries, to check a boxed capture is freed exactly once.
struct DestroyCounter {
  int *destroyed;
  bool live{true};
  explicit DestroyCounter(int *count) : destroyed(count) {}
  DestroyCounter(const DestroyCounter &other) = delete;
  DestroyCounter(DestroyCounter &&other) noexcept : destroyed(other.destroyed) { other.live = false; }
  DestroyCounter &operator=(const DestroyCounter &other) = delete;
  DestroyCounter &operator=(DestroyCounter &&other) = delete;
  ~DestroyCounter() {
    if (this->live)
      (*this->destroyed)++;
  }
};

template<typename F> bool fits_inline(const F & /*callable*/) { return IntCallback::fits_inline<F>(); }

TEST(CallbackFitsInline, PointerSizedConstCallableFits) {
  int value = 0;
  EXPECT_TRUE(fits_inline([&value](int x) { value = x; }));
}

TEST(CallbackFitsInline, MutableCallableDoesNotFit) {
  int calls = 0;
  EXPECT_FALSE(fits_inline([calls](int) mutable { calls++; }));
}

TEST(CallbackFitsInline, LargeCallableDoesNotFit) {
  int a = 0;
  int b = 0;
  EXPECT_FALSE(fits_inline([&a, &b](int x) { a = b = x; }));
}

TEST(CallbackCreate, InlineCallableIsCalled) {
  int value = 0;
  auto cb = IntCallback::create([&value](int x) { value = x; });
  cb.call(42);
  EXPECT_EQ(value, 42);
}

int mutable_seen = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// A pointer-sized mutable lambda used to be stored inline and run on a copy, losing its state.
TEST(CallbackCreate, MutableStateIsKeptAcrossCalls) {
  auto counter = [n = 0](int x) mutable { mutable_seen = (n += x); };
  static_assert(sizeof(counter) <= sizeof(void *) && std::is_trivially_copyable_v<decltype(counter)>);
  // create() never frees its heap path; held for the whole run like a callback registered in setup().
  static auto cb = IntCallback::create(counter);
  cb.call(1);
  cb.call(2);
  EXPECT_EQ(mutable_seen, 3);
}

TEST(CallbackCreateBoxed, MutableStateIsKeptAcrossCalls) {
  int seen = 0;
  auto cb = IntCallback::create_boxed([count = 0, &seen](int x) mutable {
    count += x;
    seen = count;
  });
  cb.call(1);
  cb.call(2);
  cb.call(3);
  EXPECT_EQ(seen, 6);
  cb.free_boxed();
}

TEST(CallbackCreateBoxed, FreeDestroysCaptureExactlyOnce) {
  int destroyed = 0;
  int calls = 0;
  {
    auto cb = IntCallback::create_boxed([counter = DestroyCounter(&destroyed), &calls](int) { calls++; });
    EXPECT_EQ(destroyed, 0);
    cb.call(0);
    cb.call(0);
    EXPECT_EQ(destroyed, 0);
    cb.free_boxed();
    EXPECT_EQ(destroyed, 1);
  }
  // The Callback itself owns nothing, so going out of scope frees nothing more
  EXPECT_EQ(destroyed, 1);
  EXPECT_EQ(calls, 2);
}

}  // namespace esphome::core::testing
