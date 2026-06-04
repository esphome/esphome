#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>
#include <concepts>
#include <strings.h>

#include "esphome/core/optional.h"
#include "esphome/core/time_conversion.h"

// Backward compatibility re-export of heap-allocating helpers.
// These functions have moved to alloc_helpers.h. External components should
// update their includes to use #include "esphome/core/alloc_helpers.h" directly.
// This re-export will be removed in 2026.11.0.
#include "esphome/core/alloc_helpers.h"

#ifdef USE_ESP8266
#include <Esp.h>
#include <pgmspace.h>
#endif

#ifdef USE_RP2040
#include <Arduino.h>
#endif

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

#if defined(USE_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#elif defined(USE_LIBRETINY)
#include <FreeRTOS.h>
#include <semphr.h>
#endif

#ifdef USE_HOST
#include <mutex>
#endif

#define HOT __attribute__((hot))
#define ESPDEPRECATED(msg, when) __attribute__((deprecated(msg)))
#define ESPHOME_ALWAYS_INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

namespace esphome {

// Forward declaration to avoid circular dependency with string_ref.h
class StringRef;

/// @name STL backports
///@{

// Keep "using" even after the removal of our backports, to avoid breaking existing code.
using std::to_string;
using std::is_trivially_copyable;
using std::make_unique;
using std::enable_if_t;
using std::clamp;
using std::is_invocable;
#if __cpp_lib_bit_cast >= 201806
using std::bit_cast;
#else
/// Convert data between types, without aliasing issues or undefined behaviour.
template<
    typename To, typename From,
    enable_if_t<sizeof(To) == sizeof(From) && is_trivially_copyable<From>::value && is_trivially_copyable<To>::value,
                int> = 0>
To bit_cast(const From &src) {
  To dst;
  memcpy(&dst, &src, sizeof(To));
  return dst;
}
#endif

// clang-format off
inline float lerp(float completion, float start, float end) = delete;  // Please use std::lerp. Notice that it has different order on arguments!
// clang-format on

// std::byteswap from C++23
template<typename T> constexpr T byteswap(T n) {
  T m;
  for (size_t i = 0; i < sizeof(T); i++)
    reinterpret_cast<uint8_t *>(&m)[i] = reinterpret_cast<uint8_t *>(&n)[sizeof(T) - 1 - i];
  return m;
}
template<> constexpr uint8_t byteswap(uint8_t n) { return n; }
#ifdef USE_LIBRETINY
// LibreTiny's Beken framework redefines __builtin_bswap functions as non-constexpr
template<> inline uint16_t byteswap(uint16_t n) { return __builtin_bswap16(n); }
template<> inline uint32_t byteswap(uint32_t n) { return __builtin_bswap32(n); }
template<> inline uint64_t byteswap(uint64_t n) { return __builtin_bswap64(n); }
template<> inline int8_t byteswap(int8_t n) { return n; }
template<> inline int16_t byteswap(int16_t n) { return __builtin_bswap16(n); }
template<> inline int32_t byteswap(int32_t n) { return __builtin_bswap32(n); }
template<> inline int64_t byteswap(int64_t n) { return __builtin_bswap64(n); }
#else
template<> constexpr uint16_t byteswap(uint16_t n) { return __builtin_bswap16(n); }
template<> constexpr uint32_t byteswap(uint32_t n) { return __builtin_bswap32(n); }
template<> constexpr uint64_t byteswap(uint64_t n) { return __builtin_bswap64(n); }
template<> constexpr int8_t byteswap(int8_t n) { return n; }
template<> constexpr int16_t byteswap(int16_t n) { return __builtin_bswap16(n); }
template<> constexpr int32_t byteswap(int32_t n) { return __builtin_bswap32(n); }
template<> constexpr int64_t byteswap(int64_t n) { return __builtin_bswap64(n); }
#endif

///@}

/// @name Container utilities
///@{

/// Lightweight read-only view over a const array stored in RODATA (will typically be in flash memory)
/// Avoids copying data from flash to RAM by keeping a pointer to the flash data.
/// Similar to std::span but with minimal overhead for embedded systems.

template<typename T> class ConstVector {
 public:
  constexpr ConstVector(const T *data, size_t size) : data_(data), size_(size) {}

  const constexpr T &operator[](size_t i) const { return data_[i]; }
  constexpr size_t size() const { return size_; }
  constexpr bool empty() const { return size_ == 0; }

 protected:
  const T *data_;
  size_t size_;
};

/// Small buffer optimization - stores data inline when small, heap-allocates for large data
/// This avoids heap fragmentation for common small allocations while supporting arbitrary sizes.
/// Memory management is encapsulated - callers just use set() and data().
template<size_t InlineSize = 8> class SmallInlineBuffer {
 public:
  SmallInlineBuffer() = default;
  ~SmallInlineBuffer() {
    if (!this->is_inline_())
      delete[] this->heap_;
  }

  // Move constructor
  SmallInlineBuffer(SmallInlineBuffer &&other) noexcept : len_(other.len_) {
    if (other.is_inline_()) {
      memcpy(this->inline_, other.inline_, this->len_);
    } else {
      this->heap_ = other.heap_;
      other.heap_ = nullptr;
    }
    other.len_ = 0;
  }

  // Move assignment
  SmallInlineBuffer &operator=(SmallInlineBuffer &&other) noexcept {
    if (this != &other) {
      if (!this->is_inline_())
        delete[] this->heap_;
      this->len_ = other.len_;
      if (other.is_inline_()) {
        memcpy(this->inline_, other.inline_, this->len_);
      } else {
        this->heap_ = other.heap_;
        other.heap_ = nullptr;
      }
      other.len_ = 0;
    }
    return *this;
  }

  // Disable copy (would need deep copy of heap data)
  SmallInlineBuffer(const SmallInlineBuffer &) = delete;
  SmallInlineBuffer &operator=(const SmallInlineBuffer &) = delete;

  /// Set buffer contents, allocating heap if needed
  void set(const uint8_t *src, size_t size) {
    // Free existing heap allocation if switching from heap to inline or different heap size
    if (!this->is_inline_() && (size <= InlineSize || size != this->len_)) {
      delete[] this->heap_;
      this->heap_ = nullptr;  // Defensive: prevent use-after-free if logic changes
    }
    // Allocate new heap buffer if needed
    if (size > InlineSize && (this->is_inline_() || size != this->len_)) {
      this->heap_ = new uint8_t[size];  // NOLINT(cppcoreguidelines-owning-memory)
    }
    this->len_ = size;
    memcpy(this->data(), src, size);
  }

  uint8_t *data() { return this->is_inline_() ? this->inline_ : this->heap_; }
  const uint8_t *data() const { return this->is_inline_() ? this->inline_ : this->heap_; }
  size_t size() const { return this->len_; }

 protected:
  bool is_inline_() const { return this->len_ <= InlineSize; }

  size_t len_{0};
  union {
    uint8_t inline_[InlineSize]{};  // Zero-init ensures clean initial state
    uint8_t *heap_;
  };
};

/// Minimal static vector - saves memory by avoiding std::vector overhead
template<typename T, size_t N> class StaticVector {
 public:
  using value_type = T;
  using iterator = typename std::array<T, N>::iterator;
  using const_iterator = typename std::array<T, N>::const_iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

 private:
  std::array<T, N> data_;  // intentionally not value-initialized to avoid memset
  size_t count_{0};

 public:
  // Default constructor
  StaticVector() = default;

  // Iterator range constructor
  template<typename InputIt> StaticVector(InputIt first, InputIt last) {
    while (first != last && count_ < N) {
      data_[count_++] = *first++;
    }
  }

  // Initializer list constructor
  StaticVector(std::initializer_list<T> init) {
    for (const auto &val : init) {
      if (count_ >= N)
        break;
      data_[count_++] = val;
    }
  }

  // Minimal vector-compatible interface - only what we actually use
  void push_back(const T &value) {
    if (count_ < N) {
      data_[count_++] = value;
    }
  }

  // Clear all elements
  void clear() { count_ = 0; }

  // Assign from iterator range
  template<typename InputIt> void assign(InputIt first, InputIt last) {
    count_ = 0;
    while (first != last && count_ < N) {
      data_[count_++] = *first++;
    }
  }

  // Return reference to next element and increment count (with bounds checking)
  T &emplace_next() {
    if (count_ >= N) {
      // Should never happen with proper size calculation
      // Return reference to last element to avoid crash
      return data_[N - 1];
    }
    return data_[count_++];
  }

  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }

  // Direct access to underlying data
  T *data() { return data_.data(); }
  const T *data() const { return data_.data(); }

  T &operator[](size_t i) { return data_[i]; }
  const T &operator[](size_t i) const { return data_[i]; }

  // For range-based for loops
  iterator begin() { return data_.begin(); }
  iterator end() { return data_.begin() + count_; }
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.begin() + count_; }

  // Reverse iterators
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

  // Conversion to std::span for compatibility with span-based APIs
  operator std::span<T>() { return std::span<T>(data_.data(), count_); }
  operator std::span<const T>() const { return std::span<const T>(data_.data(), count_); }
};

/// Fixed-size circular buffer with FIFO semantics and iteration support.
///
/// A tiny ring buffer that avoids dynamic allocations from std::deque/std::queue
/// (which can be wasteful on MCUs), while supporting iteration over queued elements.
///
/// Not thread-safe. All access (push/pop/iteration) must occur from a single
/// context, or the caller must provide external synchronization.
template<typename T, size_t N> class StaticRingBuffer {
  using index_type = std::conditional_t<(N <= std::numeric_limits<uint8_t>::max()), uint8_t, uint16_t>;

 public:
  class Iterator {
   public:
    Iterator(StaticRingBuffer *buf, index_type pos) : buf_(buf), pos_(pos) {}
    T &operator*() { return buf_->data_[(buf_->head_ + pos_) % N]; }
    Iterator &operator++() {
      ++pos_;
      return *this;
    }
    bool operator!=(const Iterator &other) const { return pos_ != other.pos_; }

   private:
    StaticRingBuffer *buf_;
    index_type pos_;
  };

  class ConstIterator {
   public:
    ConstIterator(const StaticRingBuffer *buf, index_type pos) : buf_(buf), pos_(pos) {}
    const T &operator*() const { return buf_->data_[(buf_->head_ + pos_) % N]; }
    ConstIterator &operator++() {
      ++pos_;
      return *this;
    }
    bool operator!=(const ConstIterator &other) const { return pos_ != other.pos_; }

   private:
    const StaticRingBuffer *buf_;
    index_type pos_;
  };

  bool push(const T &value) {
    if (this->count_ >= N) {
      return false;
    }
    this->data_[this->tail_] = value;
    this->tail_ = (this->tail_ + 1) % N;
    ++this->count_;
    return true;
  }

  void pop() {
    if (this->count_ > 0) {
      this->head_ = (this->head_ + 1) % N;
      --this->count_;
    }
  }

  T &front() { return this->data_[this->head_]; }
  const T &front() const { return this->data_[this->head_]; }
  index_type size() const { return this->count_; }
  bool empty() const { return this->count_ == 0; }

  /// Clear all elements (reset to empty)
  void clear() {
    this->head_ = 0;
    this->tail_ = 0;
    this->count_ = 0;
  }

  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, this->count_); }
  ConstIterator begin() const { return ConstIterator(this, 0); }
  ConstIterator end() const { return ConstIterator(this, this->count_); }

 protected:
  T data_[N];
  index_type head_{0};
  index_type tail_{0};
  index_type count_{0};
};

/// Fixed-capacity circular buffer - allocates once at runtime, never reallocates.
/// Runtime-sized equivalent of StaticRingBuffer - use when capacity is only known at initialization.
/// Supports FIFO push/pop and iteration over queued elements.
/// Not thread-safe.
template<typename T, size_t MAX_CAPACITY = std::numeric_limits<uint16_t>::max()> class FixedRingBuffer {
  using index_type = std::conditional_t<
      (MAX_CAPACITY <= std::numeric_limits<uint8_t>::max()), uint8_t,
      std::conditional_t<(MAX_CAPACITY <= std::numeric_limits<uint16_t>::max()), uint16_t, uint32_t>>;

 public:
  class Iterator {
   public:
    Iterator(FixedRingBuffer *buf, index_type pos) : buf_(buf), pos_(pos) {}
    T &operator*() { return buf_->data_[(buf_->head_ + pos_) % buf_->capacity_]; }
    Iterator &operator++() {
      ++pos_;
      return *this;
    }
    bool operator!=(const Iterator &other) const { return pos_ != other.pos_; }

   private:
    FixedRingBuffer *buf_;
    index_type pos_;
  };

  class ConstIterator {
   public:
    ConstIterator(const FixedRingBuffer *buf, index_type pos) : buf_(buf), pos_(pos) {}
    const T &operator*() const { return buf_->data_[(buf_->head_ + pos_) % buf_->capacity_]; }
    ConstIterator &operator++() {
      ++pos_;
      return *this;
    }
    bool operator!=(const ConstIterator &other) const { return pos_ != other.pos_; }

   private:
    const FixedRingBuffer *buf_;
    index_type pos_;
  };

  FixedRingBuffer() = default;
  ~FixedRingBuffer() {
    if constexpr (std::is_trivially_copyable<T>::value && std::is_trivially_default_constructible<T>::value) {
      ::operator delete(this->data_);
    } else {
      delete[] this->data_;
    }
  }

  // Disable copy
  FixedRingBuffer(const FixedRingBuffer &) = delete;
  FixedRingBuffer &operator=(const FixedRingBuffer &) = delete;

  /// Allocate capacity - can only be called once
  void init(index_type capacity) {
    if constexpr (std::is_trivially_copyable<T>::value && std::is_trivially_default_constructible<T>::value) {
      // Raw allocation without initialization (elements are written before read)
      // NOLINTNEXTLINE(bugprone-sizeof-expression)
      this->data_ = static_cast<T *>(::operator new(capacity * sizeof(T)));
    } else {
      this->data_ = new T[capacity];
    }
    this->capacity_ = capacity;
  }

  /// Push a value. Returns false if full.
  bool push(const T &value) {
    if (this->count_ >= this->capacity_)
      return false;
    this->data_[this->tail_] = value;
    this->tail_ = (this->tail_ + 1) % this->capacity_;
    ++this->count_;
    return true;
  }

  /// Push a value, overwriting the oldest if full.
  void push_overwrite(const T &value) {
    this->data_[this->tail_] = value;
    this->tail_ = (this->tail_ + 1) % this->capacity_;
    if (this->count_ >= this->capacity_) {
      // Buffer full - advance head to drop oldest, count stays at capacity
      this->head_ = this->tail_;
    } else {
      ++this->count_;
    }
  }

  /// Remove the oldest element.
  void pop() {
    if (this->count_ > 0) {
      this->head_ = (this->head_ + 1) % this->capacity_;
      --this->count_;
    }
  }

  T &front() { return this->data_[this->head_]; }
  const T &front() const { return this->data_[this->head_]; }
  index_type size() const { return this->count_; }
  bool empty() const { return this->count_ == 0; }
  index_type capacity() const { return this->capacity_; }
  bool full() const { return this->count_ == this->capacity_; }

  /// Clear all elements (reset to empty, keep capacity)
  void clear() {
    this->head_ = 0;
    this->tail_ = 0;
    this->count_ = 0;
  }

  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, this->count_); }
  ConstIterator begin() const { return ConstIterator(this, 0); }
  ConstIterator end() const { return ConstIterator(this, this->count_); }

 protected:
  T *data_{nullptr};
  index_type head_{0};
  index_type tail_{0};
  index_type count_{0};
  index_type capacity_{0};
};

/// Initialize a std::array from an initializer_list. Uses memcpy for trivially copyable types (optimal codegen),
/// falls back to element-wise copy for non-trivially copyable types (e.g. TemplatableValue).
/// N is always set by code generation — the caller is responsible for ensuring src.size() == N.
/// The debug assert is a safety net for development, not a runtime check.
template<typename T, size_t N> inline void init_array_from(std::array<T, N> &dest, std::initializer_list<T> src) {
#ifdef ESPHOME_DEBUG
  assert(src.size() == N);
#endif
  if constexpr (std::is_trivially_copyable_v<T>) {
    __builtin_memcpy(dest.data(), src.begin(), N * sizeof(T));
  } else {
    size_t i = 0;
    for (const auto &v : src) {
      dest[i++] = v;
    }
  }
}

/// Fixed-capacity vector - allocates once at runtime, never reallocates
/// This avoids std::vector template overhead (_M_realloc_insert, _M_default_append)
/// when size is known at initialization but not at compile time
template<typename T> class FixedVector {
 private:
  T *data_{nullptr};
  size_t size_{0};
  size_t capacity_{0};

  // Helper to destroy all elements without freeing memory
  void destroy_elements_() {
    // Only call destructors for non-trivially destructible types
    if constexpr (!std::is_trivially_destructible<T>::value) {
      for (size_t i = 0; i < size_; i++) {
        data_[i].~T();
      }
    }
  }

  // Helper to destroy elements and free memory
  void cleanup_() {
    if (data_ != nullptr) {
      destroy_elements_();
      // Free raw memory
      ::operator delete(data_);
    }
  }

  // Helper to reset pointers after cleanup
  void reset_() {
    data_ = nullptr;
    capacity_ = 0;
    size_ = 0;
  }

  // Helper to assign from initializer list (shared by constructor and assignment operator)
  void assign_from_initializer_list_(std::initializer_list<T> init_list) {
    init(init_list.size());
    size_t idx = 0;
    for (const auto &item : init_list) {
      new (data_ + idx) T(item);
      ++idx;
    }
    size_ = init_list.size();
  }

 public:
  FixedVector() = default;

  /// Constructor from initializer list - allocates exact size needed
  /// This enables brace initialization: FixedVector<int> v = {1, 2, 3};
  FixedVector(std::initializer_list<T> init_list) { assign_from_initializer_list_(init_list); }

  ~FixedVector() { cleanup_(); }

  // Disable copy operations (avoid accidental expensive copies)
  FixedVector(const FixedVector &) = delete;
  FixedVector &operator=(const FixedVector &) = delete;

  // Enable move semantics (allows use in move-only containers like std::vector)
  FixedVector(FixedVector &&other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.reset_();
  }

  // Allow conversion to std::vector
  operator std::vector<T>() const { return {data_, data_ + size_}; }

  FixedVector &operator=(FixedVector &&other) noexcept {
    if (this != &other) {
      // Delete our current data
      cleanup_();
      // Take ownership of other's data
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      // Leave other in valid empty state
      other.reset_();
    }
    return *this;
  }

  /// Assignment from initializer list - avoids temporary and move overhead
  /// This enables: FixedVector<int> v; v = {1, 2, 3};
  FixedVector &operator=(std::initializer_list<T> init_list) {
    cleanup_();
    reset_();
    assign_from_initializer_list_(init_list);
    return *this;
  }

  // Allocate capacity - can be called multiple times to reinit
  // IMPORTANT: After calling init(), you MUST use push_back() to add elements.
  // Direct assignment via operator[] does NOT update the size counter.
  void init(size_t n) {
    cleanup_();
    reset_();
    if (n > 0) {
      // Allocate raw memory without calling constructors
      // sizeof(T) is correct here for any type T (value types, pointers, etc.)
      // NOLINTNEXTLINE(bugprone-sizeof-expression)
      data_ = static_cast<T *>(::operator new(n * sizeof(T)));
      capacity_ = n;
    }
  }

  // Clear the vector (destroy all elements, reset size to 0, keep capacity)
  void clear() {
    destroy_elements_();
    size_ = 0;
  }

  // Release all memory (destroys elements and frees memory)
  void release() {
    cleanup_();
    reset_();
  }

  /// Add element without bounds checking
  /// Caller must ensure sufficient capacity was allocated via init()
  /// Silently ignores pushes beyond capacity (no exception or assertion)
  void push_back(const T &value) {
    if (size_ < capacity_) {
      // Use placement new to construct the object in pre-allocated memory
      new (&data_[size_]) T(value);
      size_++;
    }
  }

  /// Add element by move without bounds checking
  /// Caller must ensure sufficient capacity was allocated via init()
  /// Silently ignores pushes beyond capacity (no exception or assertion)
  void push_back(T &&value) {
    if (size_ < capacity_) {
      // Use placement new to move-construct the object in pre-allocated memory
      new (&data_[size_]) T(std::move(value));
      size_++;
    }
  }

  /// Emplace element without bounds checking - constructs in-place with arguments
  /// Caller must ensure sufficient capacity was allocated via init()
  /// Returns reference to the newly constructed element
  /// NOTE: Caller MUST ensure size_ < capacity_ before calling
  template<typename... Args> T &emplace_back(Args &&...args) {
    // Use placement new to construct the object in pre-allocated memory
    new (&data_[size_]) T(std::forward<Args>(args)...);
    size_++;
    return data_[size_ - 1];
  }

  /// Access first element (no bounds checking - matches std::vector behavior)
  /// Caller must ensure vector is not empty (size() > 0)
  T &front() { return data_[0]; }
  const T &front() const { return data_[0]; }

  /// Access last element (no bounds checking - matches std::vector behavior)
  /// Caller must ensure vector is not empty (size() > 0)
  T &back() { return data_[size_ - 1]; }
  const T &back() const { return data_[size_ - 1]; }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  size_t capacity() const { return capacity_; }
  bool full() const { return size_ == capacity_; }

  /// Access element without bounds checking (matches std::vector behavior)
  /// Caller must ensure index is valid (i < size())
  T &operator[](size_t i) { return data_[i]; }
  const T &operator[](size_t i) const { return data_[i]; }

  /// Access element with bounds checking (matches std::vector behavior)
  /// Note: No exception thrown on out of bounds - caller must ensure index is valid
  T &at(size_t i) { return data_[i]; }
  const T &at(size_t i) const { return data_[i]; }

  // Iterator support for range-based for loops
  T *begin() { return data_; }
  T *end() { return data_ + size_; }
  const T *begin() const { return data_; }
  const T *end() const { return data_ + size_; }
};

/// @brief Helper class for efficient buffer allocation - uses stack for small sizes, heap for large
/// This is useful when most operations need a small buffer but occasionally need larger ones.
/// The stack buffer avoids heap allocation in the common case, while heap fallback handles edge cases.
/// @tparam STACK_SIZE Number of elements in the stack buffer
/// @tparam T Element type (default: uint8_t)
template<size_t STACK_SIZE, typename T = uint8_t> class SmallBufferWithHeapFallback {
 public:
  explicit SmallBufferWithHeapFallback(size_t size) {
    if (size <= STACK_SIZE) {
      this->buffer_ = this->stack_buffer_;
    } else {
      this->heap_buffer_ = new T[size];
      this->buffer_ = this->heap_buffer_;
    }
  }
  ~SmallBufferWithHeapFallback() { delete[] this->heap_buffer_; }

  // Delete copy and move operations to prevent double-delete
  SmallBufferWithHeapFallback(const SmallBufferWithHeapFallback &) = delete;
  SmallBufferWithHeapFallback &operator=(const SmallBufferWithHeapFallback &) = delete;
  SmallBufferWithHeapFallback(SmallBufferWithHeapFallback &&) = delete;
  SmallBufferWithHeapFallback &operator=(SmallBufferWithHeapFallback &&) = delete;

  T *get() { return this->buffer_; }

 private:
  T stack_buffer_[STACK_SIZE];
  T *heap_buffer_{nullptr};
  T *buffer_;
};

///@}

/// @name Mathematics
///@{

/// Compute floor(log10(fabs(value))) using iterative comparison.
/// Avoids pulling in __ieee754_logf/log10f (~1KB flash).
/// Only valid for finite, non-zero values.
int8_t ilog10(float value);

/// Compute 10^exp using iterative multiplication/division.
/// Avoids pulling in powf/__ieee754_powf (~2.3KB flash) for small integer exponents.  // NOLINT
/// Matches powf(10, exp) for the int8_t exponent range used by sensor accuracy_decimals.  // NOLINT
inline float pow10_int(int8_t exp) {
  float result = 1.0f;
  if (exp >= 0) {
    for (int8_t i = 0; i < exp; i++)
      result *= 10.0f;
  } else {
    for (int8_t i = exp; i < 0; i++)
      result /= 10.0f;
  }
  return result;
}

/// Remap \p value from the range (\p min, \p max) to (\p min_out, \p max_out).
template<typename T, typename U> T remap(U value, U min, U max, T min_out, T max_out) {
  return (value - min) * (max_out - min_out) / (max - min) + min_out;
}

/// Calculate a CRC-8 checksum of \p data with size \p len.
uint8_t crc8(const uint8_t *data, uint8_t len, uint8_t crc = 0x00, uint8_t poly = 0x8C, bool msb_first = false);

/// Calculate a CRC-16 checksum of \p data with size \p len.
uint16_t crc16(const uint8_t *data, uint16_t len, uint16_t crc = 0xffff, uint16_t reverse_poly = 0xa001,
               bool refin = false, bool refout = false);
uint16_t crc16be(const uint8_t *data, uint16_t len, uint16_t crc = 0, uint16_t poly = 0x1021, bool refin = false,
                 bool refout = false);

/// Calculate a FNV-1 hash of \p str.
/// Note: FNV-1a (fnv1a_hash) is preferred for new code due to better avalanche characteristics.
uint32_t fnv1_hash(const char *str);
inline uint32_t fnv1_hash(const std::string &str) { return fnv1_hash(str.c_str()); }

/// FNV-1 32-bit offset basis
constexpr uint32_t FNV1_OFFSET_BASIS = 2166136261UL;
/// FNV-1 32-bit prime
constexpr uint32_t FNV1_PRIME = 16777619UL;

/// Extend a FNV-1 hash with an integer (hashes each byte).
template<std::integral T> constexpr uint32_t fnv1_hash_extend(uint32_t hash, T value) {
  using UnsignedT = std::make_unsigned_t<T>;
  UnsignedT uvalue = static_cast<UnsignedT>(value);
  for (size_t i = 0; i < sizeof(T); i++) {
    hash *= FNV1_PRIME;
    hash ^= (uvalue >> (i * 8)) & 0xFF;
  }
  return hash;
}
/// Extend a FNV-1 hash with additional string data.
constexpr uint32_t fnv1_hash_extend(uint32_t hash, const char *str) {
  if (str) {
    while (*str) {
      hash *= FNV1_PRIME;
      hash ^= *str++;
    }
  }
  return hash;
}
inline uint32_t fnv1_hash_extend(uint32_t hash, const std::string &str) { return fnv1_hash_extend(hash, str.c_str()); }

/// Extend a FNV-1a hash with additional string data.
constexpr uint32_t fnv1a_hash_extend(uint32_t hash, const char *str) {
  if (str) {
    while (*str) {
      hash ^= *str++;
      hash *= FNV1_PRIME;
    }
  }
  return hash;
}
inline uint32_t fnv1a_hash_extend(uint32_t hash, const std::string &str) {
  return fnv1a_hash_extend(hash, str.c_str());
}
/// Extend a FNV-1a hash with an integer (hashes each byte).
template<std::integral T> constexpr uint32_t fnv1a_hash_extend(uint32_t hash, T value) {
  using UnsignedT = std::make_unsigned_t<T>;
  UnsignedT uvalue = static_cast<UnsignedT>(value);
  for (size_t i = 0; i < sizeof(T); i++) {
    hash ^= (uvalue >> (i * 8)) & 0xFF;
    hash *= FNV1_PRIME;
  }
  return hash;
}
/// Calculate a FNV-1a hash of \p str.
constexpr uint32_t fnv1a_hash(const char *str) { return fnv1a_hash_extend(FNV1_OFFSET_BASIS, str); }
inline uint32_t fnv1a_hash(const std::string &str) { return fnv1a_hash(str.c_str()); }

// micros_to_millis<>() lives in its own lightweight header so hal.h can pull it
// in for inline millis_64() without forcing every TU that includes hal.h to
// also include the rest of helpers.h.

/// Return a random 32-bit unsigned integer.
/// Not thread-safe. Must only be called from the main loop.
/// Not suitable for cryptographic use; use random_bytes() instead.
uint32_t random_uint32();
/// Return a random float between 0 and 1.
/// Not thread-safe. Must only be called from the main loop.
/// Not suitable for cryptographic use; use random_bytes() instead.
float random_float();
/// Generate \p len random bytes using the platform's secure RNG (hardware RNG or OS CSPRNG).
/// Thread-safe. Suitable for cryptographic use.
bool random_bytes(uint8_t *data, size_t len);

///@}

/// @name Bit manipulation
///@{

/// Encode a 16-bit value given the most and least significant byte.
constexpr uint16_t encode_uint16(uint8_t msb, uint8_t lsb) {
  return (static_cast<uint16_t>(msb) << 8) | (static_cast<uint16_t>(lsb));
}
/// Encode a 24-bit value given three bytes in most to least significant byte order.
constexpr uint32_t encode_uint24(uint8_t byte1, uint8_t byte2, uint8_t byte3) {
  return (static_cast<uint32_t>(byte1) << 16) | (static_cast<uint32_t>(byte2) << 8) | (static_cast<uint32_t>(byte3));
}
/// Encode a 32-bit value given four bytes in most to least significant byte order.
constexpr uint32_t encode_uint32(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
  return (static_cast<uint32_t>(byte1) << 24) | (static_cast<uint32_t>(byte2) << 16) |
         (static_cast<uint32_t>(byte3) << 8) | (static_cast<uint32_t>(byte4));
}

/// Encode a value from its constituent bytes (from most to least significant) in an array with length sizeof(T).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> constexpr T encode_value(const uint8_t *bytes) {
  T val = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    val <<= 8;
    val |= bytes[i];
  }
  return val;
}
/// Encode a value from its constituent bytes (from most to least significant) in an std::array with length sizeof(T).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
constexpr T encode_value(const std::array<uint8_t, sizeof(T)> bytes) {
  return encode_value<T>(bytes.data());
}
/// Decode a value into its constituent bytes (from most to least significant).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
constexpr std::array<uint8_t, sizeof(T)> decode_value(T val) {
  std::array<uint8_t, sizeof(T)> ret{};
  for (size_t i = sizeof(T); i > 0; i--) {
    ret[i - 1] = val & 0xFF;
    val >>= 8;
  }
  return ret;
}

/// Reverse the order of 8 bits.
inline uint8_t reverse_bits(uint8_t x) {
  x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
  x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
  x = ((x & 0xF0) >> 4) | ((x & 0x0F) << 4);
  return x;
}
/// Reverse the order of 16 bits.
inline uint16_t reverse_bits(uint16_t x) {
  return (reverse_bits(static_cast<uint8_t>(x & 0xFF)) << 8) | reverse_bits(static_cast<uint8_t>((x >> 8) & 0xFF));
}
/// Reverse the order of 32 bits.
inline uint32_t reverse_bits(uint32_t x) {
  return (reverse_bits(static_cast<uint16_t>(x & 0xFFFF)) << 16) |
         reverse_bits(static_cast<uint16_t>((x >> 16) & 0xFFFF));
}

/// Convert a value between host byte order and big endian (most significant byte first) order.
template<typename T> constexpr T convert_big_endian(T val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return byteswap(val);
#else
  return val;
#endif
}

/// Convert a value between host byte order and little endian (least significant byte first) order.
template<typename T> constexpr T convert_little_endian(T val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return val;
#else
  return byteswap(val);
#endif
}

///@}

/// @name Strings
///@{

/// Compare strings for equality in case-insensitive manner.
bool str_equals_case_insensitive(const std::string &a, const std::string &b);
/// Compare StringRefs for equality in case-insensitive manner.
bool str_equals_case_insensitive(StringRef a, StringRef b);
/// Compare C strings for equality in case-insensitive manner (no heap allocation).
inline bool str_equals_case_insensitive(const char *a, const char *b) { return strcasecmp(a, b) == 0; }
inline bool str_equals_case_insensitive(const std::string &a, const char *b) { return strcasecmp(a.c_str(), b) == 0; }
inline bool str_equals_case_insensitive(const char *a, const std::string &b) { return strcasecmp(a, b.c_str()) == 0; }

/// Check whether a string starts with a value.
bool str_startswith(const std::string &str, const std::string &start);
/// Check whether a string ends with a value.
bool str_endswith(const std::string &str, const std::string &end);

/// Case-insensitive check if string ends with suffix (no heap allocation).
bool str_endswith_ignore_case(const char *str, size_t str_len, const char *suffix, size_t suffix_len);
inline bool str_endswith_ignore_case(const char *str, const char *suffix) {
  return str_endswith_ignore_case(str, strlen(str), suffix, strlen(suffix));
}
inline bool str_endswith_ignore_case(const std::string &str, const char *suffix) {
  return str_endswith_ignore_case(str.c_str(), str.size(), suffix, strlen(suffix));
}

// str_truncate moved to alloc_helpers.h - remove this include before 2026.11.0

// str_until, str_lower_case, str_upper_case moved to alloc_helpers.h - remove this comment before 2026.11.0

/// Convert a single char to snake_case: lowercase and space to underscore.
constexpr char to_snake_case_char(char c) { return (c == ' ') ? '_' : (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }
// str_snake_case moved to alloc_helpers.h - remove this comment before 2026.11.0

/// Sanitize a single char: keep alphanumerics, dashes, underscores; replace others with underscore.
constexpr char to_sanitized_char(char c) {
  return (c == '-' || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) ? c : '_';
}

/** Sanitize a string to buffer, keeping only alphanumerics, dashes, and underscores.
 *
 * @param buffer Output buffer to write to.
 * @param buffer_size Size of the output buffer.
 * @param str Input string to sanitize.
 * @return Pointer to buffer.
 *
 * Buffer size needed: strlen(str) + 1.
 */
char *str_sanitize_to(char *buffer, size_t buffer_size, const char *str);

/// Sanitize a string to buffer. Automatically deduces buffer size.
template<size_t N> inline char *str_sanitize_to(char (&buffer)[N], const char *str) {
  return str_sanitize_to(buffer, N, str);
}

// str_sanitize moved to alloc_helpers.h - remove this comment before 2026.11.0

/// Calculate FNV-1 hash of a string while applying snake_case + sanitize transformations.
/// This computes object_id hashes directly from names without creating an intermediate buffer.
/// IMPORTANT: Must match Python fnv1_hash_object_id() in esphome/helpers.py.
/// If you modify this function, update the Python version and tests in both places.
inline uint32_t fnv1_hash_object_id(const char *str, size_t len) {
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < len; i++) {
    hash *= FNV1_PRIME;
    // Apply snake_case (space->underscore, uppercase->lowercase) then sanitize
    hash ^= static_cast<uint8_t>(to_sanitized_char(to_snake_case_char(str[i])));
  }
  return hash;
}

// str_snprintf, str_sprintf moved to alloc_helpers.h - remove this comment before 2026.11.0

#ifdef USE_ESP8266
// ESP8266: Use vsnprintf_P to keep format strings in flash (PROGMEM)
// Format strings must be wrapped with PSTR() macro
/// Safely append formatted string to buffer, returning new position (capped at size).
/// @param buf Output buffer
/// @param size Total buffer size
/// @param pos Current position in buffer
/// @param fmt Format string (must be in PROGMEM on ESP8266)
/// @return New position after appending (capped at size on overflow)
inline size_t buf_append_printf_p(char *buf, size_t size, size_t pos, PGM_P fmt, ...) {
  if (pos >= size) {
    return size;
  }
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf_P(buf + pos, size - pos, fmt, args);
  va_end(args);
  if (written < 0) {
    return pos;  // encoding error
  }
  return std::min(pos + static_cast<size_t>(written), size);
}
#define buf_append_printf(buf, size, pos, fmt, ...) buf_append_printf_p(buf, size, pos, PSTR(fmt), ##__VA_ARGS__)
#else
/// Safely append formatted string to buffer, returning new position (capped at size).
/// Handles snprintf edge cases: negative returns (encoding errors) and truncation.
/// @param buf Output buffer
/// @param size Total buffer size
/// @param pos Current position in buffer
/// @param fmt printf-style format string
/// @return New position after appending (capped at size on overflow)
__attribute__((format(printf, 4, 5))) inline size_t buf_append_printf(char *buf, size_t size, size_t pos,
                                                                      const char *fmt, ...) {
  if (pos >= size) {
    return size;
  }
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(buf + pos, size - pos, fmt, args);
  va_end(args);
  if (written < 0) {
    return pos;  // encoding error
  }
  return std::min(pos + static_cast<size_t>(written), size);
}
#endif

#ifdef USE_ESP8266
/// Safely append a PROGMEM string to buffer, returning new position (capped at size).
/// ESP8266 internal implementation — prefer the `buf_append_str` macro which wraps
/// literals with `PSTR()` automatically so they stay in flash instead of eating RAM.
/// @param buf Output buffer
/// @param size Total buffer size
/// @param pos Current position in buffer
/// @param str PROGMEM-resident string to append (must not be null)
/// @return New position after appending; returns `size` if `pos >= size`, otherwise
///         returns at most `size - 1` because one byte is reserved for the null terminator
inline size_t buf_append_str_p(char *buf, size_t size, size_t pos, PGM_P str) {
  if (pos >= size) {
    return size;
  }
  size_t remaining = size - pos - 1;  // reserve space for null terminator
  size_t len = strnlen_P(str, remaining);
  memcpy_P(buf + pos, str, len);
  pos += len;
  buf[pos] = '\0';
  return pos;
}
/// Safely append a string to buffer, returning new position (capped at size).
/// More efficient than buf_append_printf for plain string literals.
/// On ESP8266 the literal is wrapped with PSTR() so it stays in flash.
#define buf_append_str(buf, size, pos, str) buf_append_str_p(buf, size, pos, PSTR(str))
#else
/// Safely append a string to buffer, returning new position (capped at size).
/// More efficient than buf_append_printf for plain string literals.
/// @param buf Output buffer
/// @param size Total buffer size
/// @param pos Current position in buffer
/// @param str String to append (must not be null)
/// @return New position after appending (capped at size on overflow)
inline size_t buf_append_str(char *buf, size_t size, size_t pos, const char *str) {
  if (pos >= size) {
    return size;
  }
  size_t remaining = size - pos - 1;  // reserve space for null terminator
  size_t len = 0;
  while (len < remaining && str[len] != '\0') {
    len++;
  }
  memcpy(buf + pos, str, len);
  pos += len;
  buf[pos] = '\0';
  return pos;
}
#endif

/// Concatenate a name with a separator and suffix using an efficient stack-based approach.
/// This avoids multiple heap allocations during string construction.
/// Maximum name length supported is 120 characters for friendly names.
/// @param name The base name string
/// @param sep The separator character (e.g., '-', ' ', or '.')
/// @param suffix_ptr Pointer to the suffix characters
/// @param suffix_len Length of the suffix
/// @return The concatenated string: name + sep + suffix
std::string make_name_with_suffix(const std::string &name, char sep, const char *suffix_ptr, size_t suffix_len);

/// Optimized string concatenation: name + separator + suffix (const char* overload)
/// Uses a fixed stack buffer to avoid heap allocations.
/// @param name The base name string
/// @param name_len Length of the name
/// @param sep Single character separator
/// @param suffix_ptr Pointer to the suffix characters
/// @param suffix_len Length of the suffix
/// @return The concatenated string: name + sep + suffix
std::string make_name_with_suffix(const char *name, size_t name_len, char sep, const char *suffix_ptr,
                                  size_t suffix_len);

/// Zero-allocation version: format name + separator + suffix directly into buffer.
/// @param buffer Output buffer (must have space for result + null terminator)
/// @param buffer_size Size of the output buffer
/// @param name The base name string
/// @param name_len Length of the name
/// @param sep Single character separator
/// @param suffix_ptr Pointer to the suffix characters
/// @param suffix_len Length of the suffix
/// @return Length written (excluding null terminator)
size_t make_name_with_suffix_to(char *buffer, size_t buffer_size, const char *name, size_t name_len, char sep,
                                const char *suffix_ptr, size_t suffix_len);

///@}

/// @name Parsing & formatting
///@{

/// Parse an unsigned decimal number from a null-terminated string.
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_unsigned<T>::value), int> = 0>
optional<T> parse_number(const char *str) {
  char *end = nullptr;
  unsigned long value = ::strtoul(str, &end, 10);  // NOLINT(google-runtime-int)
  if (end == str || *end != '\0' || value > std::numeric_limits<T>::max())
    return {};
  return value;
}
/// Parse an unsigned decimal number.
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_unsigned<T>::value), int> = 0>
optional<T> parse_number(const std::string &str) {
  return parse_number<T>(str.c_str());
}
/// Parse a signed decimal number from a null-terminated string.
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_signed<T>::value), int> = 0>
optional<T> parse_number(const char *str) {
  char *end = nullptr;
  signed long value = ::strtol(str, &end, 10);  // NOLINT(google-runtime-int)
  if (end == str || *end != '\0' || value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
    return {};
  return value;
}
/// Parse a signed decimal number.
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_signed<T>::value), int> = 0>
optional<T> parse_number(const std::string &str) {
  return parse_number<T>(str.c_str());
}
/// Parse a decimal floating-point number from a null-terminated string.
template<typename T, enable_if_t<(std::is_same<T, float>::value), int> = 0> optional<T> parse_number(const char *str) {
  char *end = nullptr;
  float value = ::strtof(str, &end);
  if (end == str || *end != '\0' || value == HUGE_VALF)
    return {};
  return value;
}
/// Parse a decimal floating-point number.
template<typename T, enable_if_t<(std::is_same<T, float>::value), int> = 0>
optional<T> parse_number(const std::string &str) {
  return parse_number<T>(str.c_str());
}

/** Parse bytes from a hex-encoded string into a byte array.
 *
 * When \p len is less than \p 2*count, the result is written to the back of \p data (i.e. this function treats \p str
 * as if it were padded with zeros at the front).
 *
 * @param str String to read from.
 * @param len Length of \p str (excluding optional null-terminator), is a limit on the number of characters parsed.
 * @param data Byte array to write to.
 * @param count Length of \p data.
 * @return The number of characters parsed from \p str.
 */
size_t parse_hex(const char *str, size_t len, uint8_t *data, size_t count);
/// Parse \p count bytes from the hex-encoded string \p str of at least \p 2*count characters into array \p data.
inline bool parse_hex(const char *str, uint8_t *data, size_t count) {
  return parse_hex(str, strlen(str), data, count) == 2 * count;
}
/// Parse \p count bytes from the hex-encoded string \p str of at least \p 2*count characters into array \p data.
inline bool parse_hex(const std::string &str, uint8_t *data, size_t count) {
  return parse_hex(str.c_str(), str.length(), data, count) == 2 * count;
}
/// Parse \p count bytes from the hex-encoded string \p str of at least \p 2*count characters into vector \p data.
inline bool parse_hex(const char *str, std::vector<uint8_t> &data, size_t count) {
  data.resize(count);
  return parse_hex(str, strlen(str), data.data(), count) == 2 * count;
}
/// Parse \p count bytes from the hex-encoded string \p str of at least \p 2*count characters into vector \p data.
inline bool parse_hex(const std::string &str, std::vector<uint8_t> &data, size_t count) {
  data.resize(count);
  return parse_hex(str.c_str(), str.length(), data.data(), count) == 2 * count;
}
/** Parse a hex-encoded string into an unsigned integer.
 *
 * @param str String to read from, starting with the most significant byte.
 * @param len Length of \p str (excluding optional null-terminator), is a limit on the number of characters parsed.
 */
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
optional<T> parse_hex(const char *str, size_t len) {
  T val = 0;
  if (len > 2 * sizeof(T) || parse_hex(str, len, reinterpret_cast<uint8_t *>(&val), sizeof(T)) == 0)
    return {};
  return convert_big_endian(val);
}
/// Parse a hex-encoded null-terminated string (starting with the most significant byte) into an unsigned integer.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> optional<T> parse_hex(const char *str) {
  return parse_hex<T>(str, strlen(str));
}
/// Parse a hex-encoded null-terminated string (starting with the most significant byte) into an unsigned integer.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> optional<T> parse_hex(const std::string &str) {
  return parse_hex<T>(str.c_str(), str.length());
}

/// Parse a hex character to its nibble value (0-15), returns 255 on invalid input
/// Returned by parse_hex_char() for non-hex characters.
static constexpr uint8_t INVALID_HEX_CHAR = 255;

constexpr uint8_t parse_hex_char(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return INVALID_HEX_CHAR;
}

/// Convert a nibble (0-15) to hex char with specified base ('a' for lowercase, 'A' for uppercase)
ESPHOME_ALWAYS_INLINE inline char format_hex_char(uint8_t v, char base) { return v >= 10 ? base + (v - 10) : '0' + v; }

/// Convert a nibble (0-15) to lowercase hex char
ESPHOME_ALWAYS_INLINE inline char format_hex_char(uint8_t v) { return format_hex_char(v, 'a'); }

/// Convert a nibble (0-15) to uppercase hex char (used for pretty printing)
ESPHOME_ALWAYS_INLINE inline char format_hex_pretty_char(uint8_t v) { return format_hex_char(v, 'A'); }

/// Write int8 value to buffer without modulo operations.
/// Buffer must have at least 4 bytes free. Returns pointer past last char written.
inline char *int8_to_str(char *buf, int8_t val) {
  int32_t v = val;
  if (v < 0) {
    *buf++ = '-';
    v = -v;
  }
  if (v >= 100) {
    *buf++ = '1';  // int8 max is 128, so hundreds digit is always 1
    v -= 100;
    // Must write tens digit (even if 0) after hundreds
    int32_t tens = v / 10;
    *buf++ = '0' + tens;
    v -= tens * 10;
  } else if (v >= 10) {
    int32_t tens = v / 10;
    *buf++ = '0' + tens;
    v -= tens * 10;
  }
  *buf++ = '0' + v;
  return buf;
}

/// Append a separator char and a string to a buffer, respecting remaining space.
/// Returns pointer past last char written. The buffer is always null-terminated
/// when remaining >= 1 (even on the no-room early-return), so callers always get
/// a valid C string.
inline char *buf_append_sep_str(char *buf, size_t remaining, char separator, const char *str, size_t str_len) {
  if (remaining < 2) {
    if (remaining >= 1) {
      *buf = '\0';
    }
    return buf;
  }
  *buf++ = separator;
  remaining--;
  size_t copy_len = std::min(str_len, remaining - 1);
  memcpy(buf, str, copy_len);
  buf += copy_len;
  *buf = '\0';
  return buf;
}

/// Return 10^n for small non-negative n (0-3) as uint32_t, avoiding float.
inline uint32_t small_pow10(int8_t n) { return n == 3 ? 1000 : n == 2 ? 100 : n == 1 ? 10 : 1; }

/// Minimum buffer size for uint32_to_str: 10 digits + null terminator.
static constexpr size_t UINT32_MAX_STR_SIZE = 11;

/// Write unsigned 32-bit integer to buffer (internal, no size check).
/// Buffer must have at least 10 bytes free. Returns pointer past last char written.
char *uint32_to_str_unchecked(char *buf, uint32_t val);

/// Write unsigned 32-bit integer to buffer with compile-time size check.
/// Null-terminates the output. Returns number of chars written (excluding null).
inline size_t uint32_to_str(std::span<char, UINT32_MAX_STR_SIZE> buf, uint32_t val) {
  char *end = uint32_to_str_unchecked(buf.data(), val);
  *end = '\0';
  return static_cast<size_t>(end - buf.data());
}

/// Write fractional digits with leading zeros to buffer (internal, no size check).
/// frac is the fractional value, divisor is the highest place value (e.g. 100 for 3 digits).
/// Returns pointer past last char written.
inline char *frac_to_str_unchecked(char *buf, uint32_t frac, uint32_t divisor) {
  while (divisor > 0) {
    *buf++ = '0' + static_cast<char>(frac / divisor);
    frac %= divisor;
    divisor /= 10;
  }
  return buf;
}

/// Format byte array as lowercase hex to buffer (base implementation).
char *format_hex_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t length);

/// Format byte array as lowercase hex to buffer. Automatically deduces buffer size.
/// Truncates output if data exceeds buffer capacity. Returns pointer to buffer.
template<size_t N> inline char *format_hex_to(char (&buffer)[N], const uint8_t *data, size_t length) {
  static_assert(N >= 3, "Buffer must hold at least one hex byte (3 chars)");
  return format_hex_to(buffer, N, data, length);
}

/// Format an unsigned integer in lowercased hex to buffer, starting with the most significant byte.
template<size_t N, typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
inline char *format_hex_to(char (&buffer)[N], T val) {
  static_assert(N >= sizeof(T) * 2 + 1, "Buffer too small for type");
  val = convert_big_endian(val);
  return format_hex_to(buffer, reinterpret_cast<const uint8_t *>(&val), sizeof(T));
}

/// Format std::vector<uint8_t> as lowercase hex to buffer.
template<size_t N> inline char *format_hex_to(char (&buffer)[N], const std::vector<uint8_t> &data) {
  return format_hex_to(buffer, data.data(), data.size());
}

/// Format std::array<uint8_t, M> as lowercase hex to buffer.
template<size_t N, size_t M> inline char *format_hex_to(char (&buffer)[N], const std::array<uint8_t, M> &data) {
  return format_hex_to(buffer, data.data(), data.size());
}

/// Calculate buffer size needed for format_hex_to: "XXXXXXXX...\0" = bytes * 2 + 1
constexpr size_t format_hex_size(size_t byte_count) { return byte_count * 2 + 1; }

/// Calculate buffer size needed for format_hex_prefixed_to: "0xXXXXXXXX...\0" = bytes * 2 + 3
constexpr size_t format_hex_prefixed_size(size_t byte_count) { return byte_count * 2 + 3; }

/// Format an unsigned integer as "0x" prefixed lowercase hex to buffer.
template<size_t N, typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
inline char *format_hex_prefixed_to(char (&buffer)[N], T val) {
  static_assert(N >= sizeof(T) * 2 + 3, "Buffer too small for prefixed hex");
  buffer[0] = '0';
  buffer[1] = 'x';
  val = convert_big_endian(val);
  format_hex_to(buffer + 2, N - 2, reinterpret_cast<const uint8_t *>(&val), sizeof(T));
  return buffer;
}

/// Format byte array as "0x" prefixed lowercase hex to buffer.
template<size_t N> inline char *format_hex_prefixed_to(char (&buffer)[N], const uint8_t *data, size_t length) {
  static_assert(N >= 5, "Buffer must hold at least '0x' + one hex byte + null");
  buffer[0] = '0';
  buffer[1] = 'x';
  format_hex_to(buffer + 2, N - 2, data, length);
  return buffer;
}

/// Calculate buffer size needed for format_hex_pretty_to with separator: "XX:XX:...:XX\0"
constexpr size_t format_hex_pretty_size(size_t byte_count) { return byte_count * 3; }

/** Format byte array as uppercase hex to buffer (base implementation).
 *
 * @param buffer Output buffer to write to.
 * @param buffer_size Size of the output buffer.
 * @param data Pointer to the byte array to format.
 * @param length Number of bytes in the array.
 * @param separator Character to use between hex bytes, or '\0' for no separator.
 * @return Pointer to buffer.
 *
 * Buffer size needed: length * 3 with separator (for "XX:XX:XX\0"), length * 2 + 1 without.
 */
char *format_hex_pretty_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t length, char separator = ':');

/// Format byte array as uppercase hex with separator to buffer. Automatically deduces buffer size.
template<size_t N>
inline char *format_hex_pretty_to(char (&buffer)[N], const uint8_t *data, size_t length, char separator = ':') {
  static_assert(N >= 3, "Buffer must hold at least one hex byte");
  return format_hex_pretty_to(buffer, N, data, length, separator);
}

/// Format std::vector<uint8_t> as uppercase hex with separator to buffer.
template<size_t N>
inline char *format_hex_pretty_to(char (&buffer)[N], const std::vector<uint8_t> &data, char separator = ':') {
  return format_hex_pretty_to(buffer, data.data(), data.size(), separator);
}

/// Format std::array<uint8_t, M> as uppercase hex with separator to buffer.
template<size_t N, size_t M>
inline char *format_hex_pretty_to(char (&buffer)[N], const std::array<uint8_t, M> &data, char separator = ':') {
  return format_hex_pretty_to(buffer, data.data(), data.size(), separator);
}

/// Calculate buffer size needed for format_hex_pretty_to with uint16_t data: "XXXX:XXXX:...:XXXX\0"
constexpr size_t format_hex_pretty_uint16_size(size_t count) { return count * 5; }

/**
 * Format uint16_t array as uppercase hex with separator to pre-allocated buffer.
 * Each uint16_t is formatted as 4 hex chars in big-endian order.
 *
 * @param buffer Output buffer to write to.
 * @param buffer_size Size of the output buffer.
 * @param data Pointer to uint16_t array.
 * @param length Number of uint16_t values.
 * @param separator Character to use between values, or '\0' for no separator.
 * @return Pointer to buffer.
 *
 * Buffer size needed: length * 5 with separator (for "XXXX:XXXX\0"), length * 4 + 1 without.
 */
char *format_hex_pretty_to(char *buffer, size_t buffer_size, const uint16_t *data, size_t length, char separator = ':');

/// Format uint16_t array as uppercase hex with separator to buffer. Automatically deduces buffer size.
template<size_t N>
inline char *format_hex_pretty_to(char (&buffer)[N], const uint16_t *data, size_t length, char separator = ':') {
  static_assert(N >= 5, "Buffer must hold at least one hex uint16_t");
  return format_hex_pretty_to(buffer, N, data, length, separator);
}

/// MAC address size in bytes
static constexpr size_t MAC_ADDRESS_SIZE = 6;
/// Buffer size for MAC address with separators: "XX:XX:XX:XX:XX:XX\0"
static constexpr size_t MAC_ADDRESS_PRETTY_BUFFER_SIZE = format_hex_pretty_size(MAC_ADDRESS_SIZE);
/// Buffer size for MAC address without separators: "XXXXXXXXXXXX\0"
static constexpr size_t MAC_ADDRESS_BUFFER_SIZE = MAC_ADDRESS_SIZE * 2 + 1;

/// Format MAC address as XX:XX:XX:XX:XX:XX (uppercase, colon separators)
inline char *format_mac_addr_upper(const uint8_t *mac, char *output) {
  return format_hex_pretty_to(output, MAC_ADDRESS_PRETTY_BUFFER_SIZE, mac, MAC_ADDRESS_SIZE, ':');
}

/// Format MAC address as xxxxxxxxxxxxxx (lowercase, no separators)
inline void format_mac_addr_lower_no_sep(const uint8_t *mac, char *output) {
  format_hex_to(output, MAC_ADDRESS_BUFFER_SIZE, mac, MAC_ADDRESS_SIZE);
}

// format_mac_address_pretty, format_hex (all overloads) moved to alloc_helpers.h
// Remove this comment and the template overloads below before 2026.11.0

/// Format an unsigned integer in lowercased hex, starting with the most significant byte.
/// @warning Allocates heap memory. Use format_hex_to() with a stack buffer instead.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> std::string format_hex(T val) {
  val = convert_big_endian(val);
  return format_hex(reinterpret_cast<uint8_t *>(&val), sizeof(T));
}
/// Format the std::array \p data in lowercased hex.
/// @warning Allocates heap memory. Use format_hex_to() with a stack buffer instead.
template<std::size_t N> std::string format_hex(const std::array<uint8_t, N> &data) {
  return format_hex(data.data(), data.size());
}

// format_hex_pretty (all overloads) moved to alloc_helpers.h
// Remove this comment and the template overload below before 2026.11.0

/// Format an unsigned integer in pretty-printed, human-readable hex format.
/// @warning Allocates heap memory. Use format_hex_pretty_to() with a stack buffer instead.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
std::string format_hex_pretty(T val, char separator = '.', bool show_length = true) {
  val = convert_big_endian(val);
  return format_hex_pretty(reinterpret_cast<uint8_t *>(&val), sizeof(T), separator, show_length);
}

/// Calculate buffer size needed for format_bin_to: "01234567...\0" = bytes * 8 + 1
constexpr size_t format_bin_size(size_t byte_count) { return byte_count * 8 + 1; }

/** Format byte array as binary string to buffer.
 *
 * Each byte is formatted as 8 binary digits (MSB first).
 * Truncates output if data exceeds buffer capacity.
 *
 * @param buffer Output buffer to write to.
 * @param buffer_size Size of the output buffer.
 * @param data Pointer to the byte array to format.
 * @param length Number of bytes in the array.
 * @return Pointer to buffer.
 *
 * Buffer size needed: length * 8 + 1 (use format_bin_size()).
 *
 * Example:
 * @code
 * char buf[9];  // format_bin_size(1)
 * format_bin_to(buf, sizeof(buf), data, 1);  // "10101011"
 * @endcode
 */
char *format_bin_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t length);

/// Format byte array as binary to buffer. Automatically deduces buffer size.
template<size_t N> inline char *format_bin_to(char (&buffer)[N], const uint8_t *data, size_t length) {
  static_assert(N >= 9, "Buffer must hold at least one binary byte (9 chars)");
  return format_bin_to(buffer, N, data, length);
}

/** Format an unsigned integer in binary to buffer, MSB first.
 *
 * @tparam N Buffer size (must be >= sizeof(T) * 8 + 1).
 * @tparam T Unsigned integer type.
 * @param buffer Output buffer to write to.
 * @param val The unsigned integer value to format.
 * @return Pointer to buffer.
 *
 * Example:
 * @code
 * char buf[9];  // format_bin_size(sizeof(uint8_t))
 * format_bin_to(buf, uint8_t{0xAA});  // "10101010"
 * char buf16[17];  // format_bin_size(sizeof(uint16_t))
 * format_bin_to(buf16, uint16_t{0x1234});  // "0001001000110100"
 * @endcode
 */
template<size_t N, typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
inline char *format_bin_to(char (&buffer)[N], T val) {
  static_assert(N >= sizeof(T) * 8 + 1, "Buffer too small for type");
  val = convert_big_endian(val);
  return format_bin_to(buffer, reinterpret_cast<const uint8_t *>(&val), sizeof(T));
}

// format_bin moved to alloc_helpers.h - remove this comment and template overload before 2026.11.0

/// Format an unsigned integer in binary, starting with the most significant byte.
/// @warning Allocates heap memory. Use format_bin_to() with a stack buffer instead.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> std::string format_bin(T val) {
  val = convert_big_endian(val);
  return format_bin(reinterpret_cast<uint8_t *>(&val), sizeof(T));
}

/// Return values for parse_on_off().
enum ParseOnOffState : uint8_t {
  PARSE_NONE = 0,
  PARSE_ON,
  PARSE_OFF,
  PARSE_TOGGLE,
};
/// Parse a string that contains either on, off or toggle.
ParseOnOffState parse_on_off(const char *str, const char *on = nullptr, const char *off = nullptr);

// value_accuracy_to_string moved to alloc_helpers.h - remove this comment before 2026.11.0

/// Maximum buffer size for value_accuracy formatting (float ~15 chars + space + UOM ~40 chars + null)
static constexpr size_t VALUE_ACCURACY_MAX_LEN = 64;

/// Format value with accuracy to buffer, returns chars written (excluding null)
size_t value_accuracy_to_buf(std::span<char, VALUE_ACCURACY_MAX_LEN> buf, float value, int8_t accuracy_decimals);
/// Format value with accuracy and UOM to buffer, returns chars written (excluding null)
size_t value_accuracy_with_uom_to_buf(std::span<char, VALUE_ACCURACY_MAX_LEN> buf, float value,
                                      int8_t accuracy_decimals, StringRef unit_of_measurement);

/// Derive accuracy in decimals from an increment step.
int8_t step_to_accuracy_decimals(float step);

// base64_encode (both overloads), base64_decode (vector overload) moved to alloc_helpers.h
// Remove this comment before 2026.11.0
size_t base64_decode(std::string const &encoded_string, uint8_t *buf, size_t buf_len);
size_t base64_decode(const uint8_t *encoded_data, size_t encoded_len, uint8_t *buf, size_t buf_len);

/// Decode base64/base64url string directly into vector of little-endian int32 values
/// @param base64 Base64 or base64url encoded string (both +/ and -_ accepted)
/// @param out Output vector (cleared and filled with decoded int32 values)
/// @return true if successful, false if decode failed or invalid size
bool base64_decode_int32_vector(const std::string &base64, std::vector<int32_t> &out);

///@}

/// @name Colors
///@{

/// Applies gamma correction of \p gamma to \p value.
// Remove before 2026.9.0
ESPDEPRECATED("Use LightState::gamma_correct_lut() instead. Removed in 2026.9.0.", "2026.3.0")
float gamma_correct(float value, float gamma);
/// Reverts gamma correction of \p gamma to \p value.
// Remove before 2026.9.0
ESPDEPRECATED("Use LightState::gamma_uncorrect_lut() instead. Removed in 2026.9.0.", "2026.3.0")
float gamma_uncorrect(float value, float gamma);

/// Convert \p red, \p green and \p blue (all 0-1) values to \p hue (0-360), \p saturation (0-1) and \p value (0-1).
void rgb_to_hsv(float red, float green, float blue, int &hue, float &saturation, float &value);
/// Convert \p hue (0-360), \p saturation (0-1) and \p value (0-1) to \p red, \p green and \p blue (all 0-1).
void hsv_to_rgb(int hue, float saturation, float value, float &red, float &green, float &blue);

///@}

/// @name Units
///@{

/// Convert degrees Celsius to degrees Fahrenheit.
constexpr float celsius_to_fahrenheit(float value) { return value * 1.8f + 32.0f; }
/// Convert degrees Fahrenheit to degrees Celsius.
constexpr float fahrenheit_to_celsius(float value) { return (value - 32.0f) / 1.8f; }

///@}

/// @name Utilities
/// @{

/// Lightweight type-erased callback (8 bytes on 32-bit) that avoids std::function overhead.
/// No null check, no exceptions, no heap allocation for small trivially-copyable callables.
///
/// With C++20 if constexpr, automatically detects [this] lambdas (sizeof <= sizeof(void*),
/// trivially copyable) and stores them inline. Larger callables are heap-allocated.
template<typename... X> struct Callback;

template<typename... Ts> struct Callback<void(Ts...)> {
  // The inline storage path stores callable bytes in ctx_ via memcpy.
  // sizeof equality with uintptr_t ensures void* can round-trip arbitrary bit patterns,
  // which combined with flat address spaces on all ESPHome targets means no trap representations.
  static_assert(sizeof(void *) == sizeof(std::uintptr_t), "void* must be the same size as uintptr_t");

  void (*fn_)(void *, Ts...){nullptr};
  void *ctx_{nullptr};

  /// Invoke the callback. Only valid on Callbacks created via create(), never on default-constructed instances.
  void call(Ts... args) const { this->fn_(this->ctx_, std::forward<Ts>(args)...); }

  /// Create from any callable. Small trivially-copyable callables (like [this] lambdas)
  /// are stored inline in the ctx pointer without heap allocation.
  template<typename F> static Callback create(F &&callable) {
    using DecayF = std::decay_t<F>;
    if constexpr (sizeof(DecayF) <= sizeof(void *) && std::is_trivially_copyable_v<DecayF>) {
      // Small trivial callable (e.g. [this]() { this->method(); }) - store inline in ctx.
      // Safe under C++20 (P0593R6): byte copy into aligned storage implicitly
      // creates objects of implicit-lifetime types (trivially copyable qualifies).
      Callback cb;  // fn and ctx are zero-initialized by default
      // Decay callable to a local variable first. When F is a function reference
      // (e.g. void(&)(int)), &callable would point at machine code, not a pointer variable.
      DecayF decayed = std::forward<F>(callable);
      __builtin_memcpy(&cb.ctx_, &decayed, sizeof(DecayF));
      cb.fn_ = [](void *c, Ts... args) {
        alignas(DecayF) char buf[sizeof(DecayF)];
        __builtin_memcpy(buf, &c, sizeof(DecayF));
        (*std::launder(reinterpret_cast<DecayF *>(buf)))(args...);
      };
      return cb;
    } else {
      // Large or non-trivial callable - heap allocate.
      // Intentionally never freed: callbacks in ESPHome are registered during setup()
      // and live for device lifetime. Same lifetime as the previous std::function approach.
      auto *stored = new DecayF(std::forward<F>(callable));
      return {[](void *c, Ts... args) { (*static_cast<DecayF *>(c))(args...); }, static_cast<void *>(stored)};
    }
  }
};

/// Grow a CallbackManager's backing array to exactly size+1. Defined in helpers.cpp.
void *callback_manager_grow(void *data, uint16_t size, uint16_t &capacity, size_t elem_size);

template<typename... X> class CallbackManager;

/** Helper class to allow having multiple subscribers to a callback.
 *
 * Uses a trivial-copyable-specialized container instead of std::vector to avoid
 * template bloat (_M_realloc_insert, exception-safe copies). Since Callback is
 * trivially copyable (just {fn_ptr, ctx_ptr}), reallocation is a plain memcpy.
 * Uses uint16_t for size/capacity (8 bytes on 32-bit vs 12 for std::vector).
 * Grows to exact size on each add — callbacks are registered during setup()
 * and most instances have only 1-2 callbacks, so slack capacity is wasteful.
 *
 * @tparam Ts The arguments for the callbacks, wrapped in void().
 */
template<typename... Ts> class CallbackManager<void(Ts...)> {
  using CbType = Callback<void(Ts...)>;
  static_assert(std::is_trivially_copyable_v<CbType>, "Callback must be trivially copyable");

 public:
  CallbackManager() = default;
  ~CallbackManager() { ::operator delete(this->data_); }

  // Non-copyable (would alias data_), movable (for std::map support)
  CallbackManager(const CallbackManager &) = delete;
  CallbackManager &operator=(const CallbackManager &) = delete;
  CallbackManager(CallbackManager &&other) noexcept
      : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }
  CallbackManager &operator=(CallbackManager &&other) noexcept {
    std::swap(this->data_, other.data_);
    std::swap(this->size_, other.size_);
    std::swap(this->capacity_, other.capacity_);
    return *this;
  }

  /// Add any callable. Small trivially-copyable callables (like [this] lambdas)
  /// are stored inline without heap allocation or std::function.
  template<typename F> void add(F &&callback) { this->add_(CbType::create(std::forward<F>(callback))); }

  /// Call all callbacks in this manager.
  inline void ESPHOME_ALWAYS_INLINE call(const Ts &...args) {
    if (this->size_ != 0) {
      for (auto *it = this->data_, *end = it + this->size_; it != end; ++it) {
        it->call(args...);
      }
    }
  }
  uint16_t size() const { return this->size_; }

  /// Call all callbacks in this manager.
  void operator()(const Ts &...args) { this->call(args...); }

 protected:
  template<typename...> friend class LazyCallbackManager;
  /// Non-template core to avoid code duplication per lambda type.
  /// Inline fast path; cold growth path is in helpers.cpp via callback_manager_grow().
  void add_(CbType cb) {
    if (this->size_ == this->capacity_) {
      this->data_ =
          static_cast<CbType *>(callback_manager_grow(this->data_, this->size_, this->capacity_, sizeof(CbType)));
    }
    this->data_[this->size_++] = cb;
  }
  CbType *data_{nullptr};
  uint16_t size_{0};
  uint16_t capacity_{0};
};

/** CallbackManager backed by StaticVector for compile-time-known callback counts.
 *
 * Drop-in replacement for CallbackManager that avoids std::vector template bloat
 * (_M_realloc_insert, etc.) when the maximum number of callbacks is known at compile time.
 *
 * @tparam N Maximum number of callbacks (compile-time constant, typically from cg.add_define())
 * @tparam Ts The arguments for the callbacks, wrapped in void().
 */
template<size_t N, typename... X> class StaticCallbackManager;

template<size_t N, typename... Ts> class StaticCallbackManager<N, void(Ts...)> {
 public:
  /// Add any callable. Small trivially-copyable callables (like [this] lambdas)
  /// are stored inline without heap allocation.
  template<typename F> void add(F &&callback) { this->add_(Callback<void(Ts...)>::create(std::forward<F>(callback))); }

  /// Call all callbacks in this manager.
  void call(Ts... args) {
    for (auto &cb : this->callbacks_)
      cb.call(args...);
  }
  size_t size() const { return this->callbacks_.size(); }

  /// Call all callbacks in this manager.
  void operator()(Ts... args) { call(args...); }

 protected:
  /// Non-template core to avoid code duplication per lambda type.
  void add_(Callback<void(Ts...)> cb) { this->callbacks_.push_back(cb); }
  StaticVector<Callback<void(Ts...)>, N> callbacks_;
};

template<typename... X> class LazyCallbackManager;

/** Lazy-allocating callback manager that only allocates memory when callbacks are registered.
 *
 * This is a drop-in replacement for CallbackManager that saves memory when no callbacks
 * are registered (common case after the Controller Registry eliminated per-entity callbacks
 * from API and web_server components).
 *
 * Memory overhead comparison (32-bit systems):
 * - CallbackManager: 8 bytes (pointer + uint16 size + uint16 capacity)
 * - LazyCallbackManager: 4 bytes (nullptr pointer)
 *
 * Uses plain pointer instead of unique_ptr to avoid template instantiation overhead.
 * The class is explicitly non-copyable/non-movable for Rule of Five compliance.
 *
 * @tparam Ts The arguments for the callbacks, wrapped in void().
 */
template<typename... Ts> class LazyCallbackManager<void(Ts...)> {
 public:
  LazyCallbackManager() = default;
  /// Destructor - clean up allocated CallbackManager if any.
  /// In practice this never runs (entities live for device lifetime) but included for correctness.
  ~LazyCallbackManager() { delete this->callbacks_; }

  // Non-copyable and non-movable (entities are never copied or moved)
  LazyCallbackManager(const LazyCallbackManager &) = delete;
  LazyCallbackManager &operator=(const LazyCallbackManager &) = delete;
  LazyCallbackManager(LazyCallbackManager &&) = delete;
  LazyCallbackManager &operator=(LazyCallbackManager &&) = delete;

  /// Add any callable. Allocates the underlying CallbackManager on first use.
  template<typename F> void add(F &&callback) { this->add_(Callback<void(Ts...)>::create(std::forward<F>(callback))); }

  /// Call all callbacks in this manager. No-op if no callbacks registered.
  void call(Ts... args) {
    if (this->callbacks_) {
      this->callbacks_->call(args...);
    }
  }

  /// Return the number of registered callbacks.
  size_t size() const { return this->callbacks_ ? this->callbacks_->size() : 0; }

  /// Check if any callbacks are registered.
  bool empty() const { return !this->callbacks_ || this->callbacks_->size() == 0; }

  /// Call all callbacks in this manager.
  void operator()(Ts... args) { this->call(args...); }

 protected:
  /// Non-template core to avoid code duplication per lambda type.
  void add_(Callback<void(Ts...)> cb) {
    if (!this->callbacks_) {
      this->callbacks_ = new CallbackManager<void(Ts...)>();
    }
    this->callbacks_->add_(cb);
  }
  CallbackManager<void(Ts...)> *callbacks_{nullptr};
};

/// Helper class to deduplicate items in a series of values.
template<typename T> class Deduplicator {
 public:
  /// Feeds the next item in the series to the deduplicator and returns false if this is a duplicate.
  bool next(T value) {
    if (this->has_value_ && !this->value_unknown_ && this->last_value_ == value) {
      return false;
    }
    this->has_value_ = true;
    this->value_unknown_ = false;
    this->last_value_ = value;
    return true;
  }
  /// Returns true if the deduplicator's value was previously known.
  bool next_unknown() {
    bool ret = !this->value_unknown_;
    this->value_unknown_ = true;
    return ret;
  }
  /// Returns true if this deduplicator has processed any items.
  bool has_value() const { return this->has_value_; }

 protected:
  bool has_value_{false};
  bool value_unknown_{false};
  T last_value_{};
};

/// Helper class to easily give an object a parent of type \p T.
template<typename T> class Parented {
 public:
  Parented() {}
  Parented(T *parent) : parent_(parent) {}

  /// Get the parent of this object.
  T *get_parent() const { return parent_; }
  /// Set the parent of this object.
  void set_parent(T *parent) { parent_ = parent; }

 protected:
  T *parent_{nullptr};
};

/// @}

/// @name System APIs
///@{

/** Mutex implementation, with API based on the unavailable std::mutex.
 *
 * @note This mutex is non-recursive, so take care not to try to obtain the mutex while it is already taken.
 */
class Mutex {
 public:
  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

#if defined(USE_ESP8266) || defined(USE_RP2040)
  // Single-threaded platforms: inline no-ops so the compiler eliminates all call overhead.
  Mutex() = default;
  ~Mutex() = default;
  void lock() {}
  bool try_lock() { return true; }
  void unlock() {}
#elif defined(USE_ESP32) || defined(USE_LIBRETINY)
  // FreeRTOS platforms: inline to avoid out-of-line call overhead.
  Mutex() { handle_ = xSemaphoreCreateMutex(); }
  ~Mutex() = default;
  void lock() { xSemaphoreTake(this->handle_, portMAX_DELAY); }
  bool try_lock() { return xSemaphoreTake(this->handle_, 0) == pdTRUE; }
  void unlock() { xSemaphoreGive(this->handle_); }

 private:
  SemaphoreHandle_t handle_;
#else
  Mutex();
  ~Mutex();
  void lock();
  bool try_lock();
  void unlock();

 private:
  // d-pointer to store private data on new platforms
  void *handle_;  // NOLINT(clang-diagnostic-unused-private-field)
#endif
};

/** Helper class that wraps a mutex with a RAII-style API.
 *
 * This behaves like std::lock_guard: as long as the object is alive, the mutex is held.
 */
class LockGuard {
 public:
  LockGuard(Mutex &mutex) : mutex_(mutex) { mutex_.lock(); }
  ~LockGuard() { mutex_.unlock(); }

 private:
  Mutex &mutex_;
};

/** Helper class to disable interrupts.
 *
 * This behaves like std::lock_guard: as long as the object is alive, all interrupts are disabled.
 *
 * Please note all functions called when the interrupt lock must be marked IRAM_ATTR (loading code into
 * instruction cache is done via interrupts; disabling interrupts prevents data not already in cache from being
 * pulled from flash).
 *
 * Example usage:
 *
 * \code{.cpp}
 * // interrupts are enabled
 * {
 *   InterruptLock lock;
 *   // do something
 *   // interrupts are disabled
 * }
 * // interrupts are enabled
 * \endcode
 */
class InterruptLock {
 public:
  InterruptLock();
  ~InterruptLock();

 protected:
#if defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_ZEPHYR)
  uint32_t state_;
#endif
};

/** Helper class to lock the lwIP TCPIP core when making lwIP API calls from non-TCPIP threads.
 *
 * This is needed on multi-threaded platforms (ESP32) when CONFIG_LWIP_TCPIP_CORE_LOCKING is enabled,
 * and on RP2040 when CYW43 WiFi is active (cyw43_arch_lwip_begin/end).
 *
 * On platforms without lwIP core locking (ESP8266, LibreTiny, Zephyr),
 * this is a no-op defined inline so the compiler can eliminate all call overhead.
 */
class LwIPLock {
 public:
  LwIPLock(const LwIPLock &) = delete;
  LwIPLock &operator=(const LwIPLock &) = delete;

#if defined(USE_ESP32) || defined(USE_RP2040)
  // Platforms with potential lwIP core locking — out-of-line implementations in helpers.cpp
  LwIPLock();
  ~LwIPLock();
#else
  // No lwIP core locking — inline no-ops (empty bodies instead of = default
  // to prevent clang-tidy unused-variable warnings at call sites)
  LwIPLock() {}
  ~LwIPLock() {}
#endif
};

/** Helper class to request `loop()` to be called as fast as possible.
 *
 * Usually the ESPHome main loop runs at 60 Hz, sleeping in between invocations of `loop()` if necessary. When a higher
 * execution frequency is necessary, you can use this class to make the loop run continuously without waiting.
 */
class HighFrequencyLoopRequester {
 public:
  /// Start running the loop continuously.
  void start();
  /// Stop running the loop continuously.
  void stop();

  /// Check whether the loop is running continuously.
  static bool is_high_frequency() { return num_requests > 0; }

 protected:
  bool started_{false};
  static uint8_t num_requests;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

/// Get the device MAC address as raw bytes, written into the provided byte array (6 bytes).
void get_mac_address_raw(uint8_t *mac);  // NOLINT(readability-non-const-parameter)

// get_mac_address, get_mac_address_pretty moved to alloc_helpers.h - remove this comment before 2026.11.0

/// Get the device MAC address into the given buffer, in lowercase hex notation.
/// Assumes buffer length is MAC_ADDRESS_BUFFER_SIZE (12 digits for hexadecimal representation followed by null
/// terminator).
void get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_BUFFER_SIZE> buf);

/// Get the device MAC address into the given buffer, in colon-separated uppercase hex notation.
/// Buffer must be exactly MAC_ADDRESS_PRETTY_BUFFER_SIZE bytes (17 for "XX:XX:XX:XX:XX:XX" + null terminator).
/// Returns pointer to the buffer for convenience.
const char *get_mac_address_pretty_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf);

#ifdef USE_ESP32
/// Set the MAC address to use from the provided byte array (6 bytes).
void set_mac_address(uint8_t *mac);
#endif

/// Check if a custom MAC address is set (ESP32 & variants)
/// @return True if a custom MAC address is set (ESP32 & variants), else false
bool has_custom_mac_address();

/// Check if the MAC address is not all zeros or all ones
/// @return True if MAC is valid, else false
bool mac_address_is_valid(const uint8_t *mac);

/// Delay for the given amount of microseconds, possibly yielding to other processes during the wait.
void delay_microseconds_safe(uint32_t us);

///@}

/// @name Memory management
///@{

/** An STL allocator that uses SPI or internal RAM.
 * Returns `nullptr` in case no memory is available.
 *
 * By setting flags, it can be configured to:
 * - perform external allocation falling back to internal memory if SPI RAM is full or unavailable (default)
 * - perform internal allocation falling back to external memory (with PREFER_INTERNAL)
 * - perform external allocation only
 * - perform internal allocation only
 */
template<class T> class RAMAllocator {
 public:
  using value_type = T;

  enum Flags {
    NONE = 0,                  // Perform external allocation and fall back to internal memory
    ALLOC_EXTERNAL = 1 << 0,   // Perform external allocation only.
    ALLOC_INTERNAL = 1 << 1,   // Perform internal allocation only.
    ALLOW_FAILURE = 1 << 2,    // Does nothing. Kept for compatibility.
    PREFER_INTERNAL = 1 << 3,  // Perform internal allocation and fall back to external memory
  };

  constexpr RAMAllocator() = default;
  constexpr RAMAllocator(uint8_t flags) {
    if (flags & PREFER_INTERNAL) {
      this->flags_ = ALLOC_INTERNAL | ALLOC_EXTERNAL | PREFER_INTERNAL;
      return;
    }
    const uint8_t alloc_bits = flags & (ALLOC_INTERNAL | ALLOC_EXTERNAL);
    if (alloc_bits != 0) {
      this->flags_ = alloc_bits;
      return;
    }
    this->flags_ = ALLOC_INTERNAL | ALLOC_EXTERNAL;
  }
  template<class U> constexpr RAMAllocator(const RAMAllocator<U> &other) : flags_{other.flags_} {}

  T *allocate(size_t n) { return this->allocate(n, sizeof(T)); }

  T *allocate(size_t n, size_t manual_size) {
    size_t size = n * manual_size;
    T *ptr = nullptr;
#ifdef USE_ESP32
    const auto caps = this->get_caps_();
    ptr = static_cast<T *>(heap_caps_malloc_prefer(size, 2, caps[0], caps[1]));
#else
    // Ignore ALLOC_EXTERNAL/ALLOC_INTERNAL flags if external allocation is not supported
    ptr = static_cast<T *>(malloc(size));  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
#endif
    return ptr;
  }

  T *reallocate(T *p, size_t n) { return this->reallocate(p, n, sizeof(T)); }

  T *reallocate(T *p, size_t n, size_t manual_size) {
    size_t size = n * manual_size;
    T *ptr = nullptr;
#ifdef USE_ESP32
    const auto caps = this->get_caps_();
    ptr = static_cast<T *>(heap_caps_realloc_prefer(p, size, 2, caps[0], caps[1]));
#else
    // Ignore ALLOC_EXTERNAL/ALLOC_INTERNAL flags if external allocation is not supported
    ptr = static_cast<T *>(realloc(p, size));  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
#endif
    return ptr;
  }

  void deallocate(T *p, size_t n) {
    free(p);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  }

  /**
   * Return the total heap space available via this allocator
   */
  size_t get_free_heap_size() const {
#ifdef USE_ESP8266
    return ESP.getFreeHeap();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP32)
    auto max_internal =
        this->flags_ & ALLOC_INTERNAL ? heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL) : 0;
    auto max_external =
        this->flags_ & ALLOC_EXTERNAL ? heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM) : 0;
    return max_internal + max_external;
#elif defined(USE_RP2040)
    return ::rp2040.getFreeHeap();
#elif defined(USE_LIBRETINY)
    return lt_heap_get_free();
#else
    return 100000;
#endif
  }

  /**
   * Return the maximum size block this allocator could allocate. This may be an approximation on some platforms
   */
  size_t get_max_free_block_size() const {
#ifdef USE_ESP8266
    return ESP.getMaxFreeBlockSize();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP32)
    auto max_internal =
        this->flags_ & ALLOC_INTERNAL ? heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL) : 0;
    auto max_external =
        this->flags_ & ALLOC_EXTERNAL ? heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM) : 0;
    return std::max(max_internal, max_external);
#else
    return this->get_free_heap_size();
#endif
  }

 private:
#ifdef USE_ESP32
  /// Returns {primary_caps, fallback_caps} for heap_caps_*_prefer based on the configured flags.
  /// PREFER_INTERNAL implies both regions are enabled (enforced by the constructor), so when it is set
  /// the primary is internal and the fallback is external. Otherwise the primary is whichever region
  /// is enabled (external preferred when both are enabled), and the fallback is the other region (or
  /// the same region when only one is enabled, making the second attempt a no-op).
  std::array<uint32_t, 2> get_caps_() const {
    constexpr uint32_t external_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    constexpr uint32_t internal_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    if (this->flags_ & PREFER_INTERNAL) {
      return {internal_caps, external_caps};
    }
    const uint32_t primary = (this->flags_ & ALLOC_EXTERNAL) ? external_caps : internal_caps;
    const uint32_t fallback = (this->flags_ & ALLOC_INTERNAL) ? internal_caps : external_caps;
    return {primary, fallback};
  }
#endif

  uint8_t flags_{ALLOC_INTERNAL | ALLOC_EXTERNAL};
};

template<class T> using ExternalRAMAllocator = RAMAllocator<T>;

/**
 * Functions to constrain the range of arithmetic values.
 */

template<typename T, typename U>
concept comparable_with = requires(T a, U b) {
  { a > b } -> std::convertible_to<bool>;
  { a < b } -> std::convertible_to<bool>;
};

template<std::totally_ordered T, comparable_with<T> U> T clamp_at_least(T value, U min) {
  if (value < min)
    return min;
  return value;
}
template<std::totally_ordered T, comparable_with<T> U> T clamp_at_most(T value, U max) {
  if (value > max)
    return max;
  return value;
}

/// @name Internal functions
///@{

/** Helper function to make `id(var)` known from lambdas work in custom components.
 *
 * This function is not called from lambdas, the code generator replaces calls to it with the appropriate variable.
 */
template<typename T, enable_if_t<!std::is_pointer<T>::value, int> = 0> T id(T value) { return value; }
/** Helper function to make `id(var)` known from lambdas work in custom components.
 *
 * This function is not called from lambdas, the code generator replaces calls to it with the appropriate variable.
 */
template<typename T, enable_if_t<std::is_pointer<T *>::value, int> = 0> T &id(T *value) { return *value; }

///@}

}  // namespace esphome
