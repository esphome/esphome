#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/core/defines.h"
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

TEST(LazyCallbackManager, CallWithoutCallbacksDoesNothing) {
  LazyCallbackManager<void(int)> manager;
  EXPECT_TRUE(manager.empty());
  EXPECT_EQ(manager.size(), 0u);
  manager.call(1);
  manager(2);
  EXPECT_TRUE(manager.empty());
}

TEST(LazyCallbackManager, CallRunsEveryCallbackInOrder) {
  LazyCallbackManager<void(int)> manager;
  std::vector<int> seen;
  manager.add([&seen](int x) { seen.push_back(x); });
  manager.call(1);
  EXPECT_EQ(seen, (std::vector<int>{1}));
  manager.add([&seen](int x) { seen.push_back(x * 10); });
  EXPECT_FALSE(manager.empty());
  EXPECT_EQ(manager.size(), 2u);
  manager.call(2);
  manager(3);
  EXPECT_EQ(seen, (std::vector<int>{1, 2, 20, 3, 30}));
}

// Not trivially copyable, so LazyCallbackManager borrows it instead of copying it into call_all().
// The callbacks take a mutable reference so clang-tidy does not flag the by-value Callback parameters.
struct CopyCounter {
  static int copies;
  CopyCounter() = default;
  CopyCounter(const CopyCounter & /*other*/) { ++copies; }
  CopyCounter(CopyCounter &&) noexcept = default;
};
int CopyCounter::copies = 0;

TEST(LazyCallbackManager, NonTrivialArgumentIsNotCopiedMoreThanCallbackManager) {
  CopyCounter arg;
  CallbackManager<void(CopyCounter)> plain;
  plain.add([](CopyCounter & /*c*/) {});
  CopyCounter::copies = 0;
  plain.call(arg);
  const int plain_copies = CopyCounter::copies;

  LazyCallbackManager<void(CopyCounter)> lazy;
  lazy.add([](CopyCounter & /*c*/) {});
  CopyCounter::copies = 0;
  lazy.call(arg);
  // The one extra copy is the by-value parameter of call(), as before call_all() existed.
  EXPECT_EQ(CopyCounter::copies, plain_copies + 1);
}

// The argument type may still be incomplete where the manager is declared; it is only checked by call().
struct SelfEvent {
  LazyCallbackManager<void(SelfEvent)> callbacks;
};
struct LateEvent;
struct LateEventSource {
  LazyCallbackManager<void(LateEvent)> callbacks;
};
struct LateEvent {
  int value;
};

TEST(LazyCallbackManager, ArgumentTypeMayBeIncompleteAtDeclaration) {
  LateEventSource source;
  int seen = 0;
  source.callbacks.add([&seen](LateEvent e) { seen = e.value; });
  source.callbacks.call(LateEvent{7});
  EXPECT_EQ(seen, 7);
}

// Exposes where the manager keeps its callbacks.
template<typename T> class PeekLazyCallbackManager : public LazyCallbackManager<T> {
 public:
  const void *storage() const { return this->callbacks_; }
};

TEST(LazyCallbackManager, StaticSlotKeepsCallbacksInOrder) {
  PeekLazyCallbackManager<void(int)> manager;
  std::vector<int> calls;
  manager.add([&calls](int x) { calls.push_back(x); });
  const void *storage = manager.storage();
  // The core test manifest reserves more slots than any test build uses
  ASSERT_TRUE(callback_manager_is_static(storage));
  manager.add([&calls](int x) { calls.push_back(x * 10); });
  EXPECT_EQ(manager.storage(), storage);
  EXPECT_EQ(manager.size(), 2u);
  EXPECT_FALSE(manager.empty());
  manager.call(3);
  EXPECT_EQ(calls, (std::vector<int>{3, 30}));
}

TEST(LazyCallbackManager, HeapOnceThePoolRunsOut) {
  std::array<PeekLazyCallbackManager<void(int)>, ESPHOME_CALLBACK_MANAGER_POOL_SIZE + 1> managers;
  int sum = 0;
  bool heap_seen = false;
  for (auto &manager : managers) {
    manager.add([&sum](int x) { sum += x; });
    if (callback_manager_is_static(manager.storage())) {
      EXPECT_FALSE(heap_seen);
    } else {
      heap_seen = true;
    }
    manager.call(1);
  }
  EXPECT_TRUE(heap_seen);
  EXPECT_EQ(sum, ESPHOME_CALLBACK_MANAGER_POOL_SIZE + 1);
}

TEST(LazyCallbackManager, StackObjectIsNotStatic) {
  CallbackManager<void(int)> local;
  EXPECT_FALSE(callback_manager_is_static(&local));
  EXPECT_FALSE(callback_manager_is_static(nullptr));
}

}  // namespace esphome::core::testing
