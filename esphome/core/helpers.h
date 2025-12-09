#pragma once

#include <array>
#include <cmath>
#include <cstdint>
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

#include "esphome/core/optional.h"

#ifdef USE_ESP8266
#include <Esp.h>
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

/// Minimal static vector - saves memory by avoiding std::vector overhead
template<typename T, size_t N> class StaticVector {
 public:
  using value_type = T;
  using iterator = typename std::array<T, N>::iterator;
  using const_iterator = typename std::array<T, N>::const_iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

 private:
  std::array<T, N> data_{};
  size_t count_{0};

 public:
  // Minimal vector-compatible interface - only what we actually use
  void push_back(const T &value) {
    if (count_ < N) {
      data_[count_++] = value;
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
};

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

  // Shrink capacity to fit current size (frees all memory)
  void shrink_to_fit() {
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

///@}

/// @name Mathematics
///@{

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
uint32_t fnv1_hash(const char *str);
inline uint32_t fnv1_hash(const std::string &str) { return fnv1_hash(str.c_str()); }

/// Return a random 32-bit unsigned integer.
uint32_t random_uint32();
/// Return a random float between 0 and 1.
float random_float();
/// Generate \p len number of random bytes.
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

/// Check whether a string starts with a value.
bool str_startswith(const std::string &str, const std::string &start);
/// Check whether a string ends with a value.
bool str_endswith(const std::string &str, const std::string &end);

/// Truncate a string to a specific length.
std::string str_truncate(const std::string &str, size_t length);

/// Extract the part of the string until either the first occurrence of the specified character, or the end
/// (requires str to be null-terminated).
std::string str_until(const char *str, char ch);
/// Extract the part of the string until either the first occurrence of the specified character, or the end.
std::string str_until(const std::string &str, char ch);

/// Convert the string to lower case.
std::string str_lower_case(const std::string &str);
/// Convert the string to upper case.
std::string str_upper_case(const std::string &str);
/// Convert the string to snake case (lowercase with underscores).
std::string str_snake_case(const std::string &str);

/// Sanitizes the input string by removing all characters but alphanumerics, dashes and underscores.
std::string str_sanitize(const std::string &str);

/// snprintf-like function returning std::string of maximum length \p len (excluding null terminator).
std::string __attribute__((format(printf, 1, 3))) str_snprintf(const char *fmt, size_t len, ...);

/// sprintf-like function returning std::string.
std::string __attribute__((format(printf, 1, 2))) str_sprintf(const char *fmt, ...);

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

/// Convert a nibble (0-15) to lowercase hex char
inline char format_hex_char(uint8_t v) { return v >= 10 ? 'a' + (v - 10) : '0' + v; }

/// Convert a nibble (0-15) to uppercase hex char (used for pretty printing)
/// This always uses uppercase (A-F) for pretty/human-readable output
inline char format_hex_pretty_char(uint8_t v) { return v >= 10 ? 'A' + (v - 10) : '0' + v; }

/// Format MAC address as XX:XX:XX:XX:XX:XX (uppercase)
inline void format_mac_addr_upper(const uint8_t *mac, char *output) {
  for (size_t i = 0; i < 6; i++) {
    uint8_t byte = mac[i];
    output[i * 3] = format_hex_pretty_char(byte >> 4);
    output[i * 3 + 1] = format_hex_pretty_char(byte & 0x0F);
    if (i < 5)
      output[i * 3 + 2] = ':';
  }
  output[17] = '\0';
}

/// Format MAC address as xxxxxxxxxxxxxx (lowercase, no separators)
inline void format_mac_addr_lower_no_sep(const uint8_t *mac, char *output) {
  for (size_t i = 0; i < 6; i++) {
    uint8_t byte = mac[i];
    output[i * 2] = format_hex_char(byte >> 4);
    output[i * 2 + 1] = format_hex_char(byte & 0x0F);
  }
  output[12] = '\0';
}

/// Format the six-byte array \p mac into a MAC address.
std::string format_mac_address_pretty(const uint8_t mac[6]);
/// Format the byte array \p data of length \p len in lowercased hex.
std::string format_hex(const uint8_t *data, size_t length);
/// Format the vector \p data in lowercased hex.
std::string format_hex(const std::vector<uint8_t> &data);
/// Format an unsigned integer in lowercased hex, starting with the most significant byte.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> std::string format_hex(T val) {
  val = convert_big_endian(val);
  return format_hex(reinterpret_cast<uint8_t *>(&val), sizeof(T));
}
template<std::size_t N> std::string format_hex(const std::array<uint8_t, N> &data) {
  return format_hex(data.data(), data.size());
}

/** Format a byte array in pretty-printed, human-readable hex format.
 *
 * Converts binary data to a hexadecimal string representation with customizable formatting.
 * Each byte is displayed as a two-digit uppercase hex value, separated by the specified separator.
 * Optionally includes the total byte count in parentheses at the end.
 *
 * @param data Pointer to the byte array to format.
 * @param length Number of bytes in the array.
 * @param separator Character to use between hex bytes (default: '.').
 * @param show_length Whether to append the byte count in parentheses (default: true).
 * @return Formatted hex string, e.g., "A1.B2.C3.D4.E5 (5)" or "A1:B2:C3" depending on parameters.
 *
 * @note Returns empty string if data is nullptr or length is 0.
 * @note The length will only be appended if show_length is true AND the length is greater than 4.
 *
 * Example:
 * @code
 * uint8_t data[] = {0xA1, 0xB2, 0xC3};
 * format_hex_pretty(data, 3);           // Returns "A1.B2.C3" (no length shown for <= 4 parts)
 * uint8_t data2[] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5};
 * format_hex_pretty(data2, 5);          // Returns "A1.B2.C3.D4.E5 (5)"
 * format_hex_pretty(data2, 5, ':');     // Returns "A1:B2:C3:D4:E5 (5)"
 * format_hex_pretty(data2, 5, '.', false); // Returns "A1.B2.C3.D4.E5"
 * @endcode
 */
std::string format_hex_pretty(const uint8_t *data, size_t length, char separator = '.', bool show_length = true);

/** Format a 16-bit word array in pretty-printed, human-readable hex format.
 *
 * Similar to the byte array version, but formats 16-bit words as 4-digit hex values.
 *
 * @param data Pointer to the 16-bit word array to format.
 * @param length Number of 16-bit words in the array.
 * @param separator Character to use between hex words (default: '.').
 * @param show_length Whether to append the word count in parentheses (default: true).
 * @return Formatted hex string with 4-digit hex values per word.
 *
 * @note The length will only be appended if show_length is true AND the length is greater than 4.
 *
 * Example:
 * @code
 * uint16_t data[] = {0xA1B2, 0xC3D4};
 * format_hex_pretty(data, 2); // Returns "A1B2.C3D4" (no length shown for <= 4 parts)
 * uint16_t data2[] = {0xA1B2, 0xC3D4, 0xE5F6};
 * format_hex_pretty(data2, 3); // Returns "A1B2.C3D4.E5F6 (3)"
 * @endcode
 */
std::string format_hex_pretty(const uint16_t *data, size_t length, char separator = '.', bool show_length = true);

/** Format a byte vector in pretty-printed, human-readable hex format.
 *
 * Convenience overload for std::vector<uint8_t>. Formats each byte as a two-digit
 * uppercase hex value with customizable separator.
 *
 * @param data Vector of bytes to format.
 * @param separator Character to use between hex bytes (default: '.').
 * @param show_length Whether to append the byte count in parentheses (default: true).
 * @return Formatted hex string representation of the vector contents.
 *
 * @note The length will only be appended if show_length is true AND the vector size is greater than 4.
 *
 * Example:
 * @code
 * std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
 * format_hex_pretty(data);        // Returns "DE.AD.BE.EF" (no length shown for <= 4 parts)
 * std::vector<uint8_t> data2 = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA};
 * format_hex_pretty(data2);       // Returns "DE.AD.BE.EF.CA (5)"
 * format_hex_pretty(data2, '-');  // Returns "DE-AD-BE-EF-CA (5)"
 * @endcode
 */
std::string format_hex_pretty(const std::vector<uint8_t> &data, char separator = '.', bool show_length = true);

/** Format a 16-bit word vector in pretty-printed, human-readable hex format.
 *
 * Convenience overload for std::vector<uint16_t>. Each 16-bit word is formatted
 * as a 4-digit uppercase hex value in big-endian order.
 *
 * @param data Vector of 16-bit words to format.
 * @param separator Character to use between hex words (default: '.').
 * @param show_length Whether to append the word count in parentheses (default: true).
 * @return Formatted hex string representation of the vector contents.
 *
 * @note The length will only be appended if show_length is true AND the vector size is greater than 4.
 *
 * Example:
 * @code
 * std::vector<uint16_t> data = {0x1234, 0x5678};
 * format_hex_pretty(data); // Returns "1234.5678" (no length shown for <= 4 parts)
 * std::vector<uint16_t> data2 = {0x1234, 0x5678, 0x9ABC};
 * format_hex_pretty(data2); // Returns "1234.5678.9ABC (3)"
 * @endcode
 */
std::string format_hex_pretty(const std::vector<uint16_t> &data, char separator = '.', bool show_length = true);

/** Format a string's bytes in pretty-printed, human-readable hex format.
 *
 * Treats each character in the string as a byte and formats it in hex.
 * Useful for debugging binary data stored in std::string containers.
 *
 * @param data String whose bytes should be formatted as hex.
 * @param separator Character to use between hex bytes (default: '.').
 * @param show_length Whether to append the byte count in parentheses (default: true).
 * @return Formatted hex string representation of the string's byte contents.
 *
 * @note The length will only be appended if show_length is true AND the string length is greater than 4.
 *
 * Example:
 * @code
 * std::string data = "ABC";  // ASCII: 0x41, 0x42, 0x43
 * format_hex_pretty(data);   // Returns "41.42.43" (no length shown for <= 4 parts)
 * std::string data2 = "ABCDE";
 * format_hex_pretty(data2);  // Returns "41.42.43.44.45 (5)"
 * @endcode
 */
std::string format_hex_pretty(const std::string &data, char separator = '.', bool show_length = true);

/** Format an unsigned integer in pretty-printed, human-readable hex format.
 *
 * Converts the integer to big-endian byte order and formats each byte as hex.
 * The most significant byte appears first in the output string.
 *
 * @tparam T Unsigned integer type (uint8_t, uint16_t, uint32_t, uint64_t, etc.).
 * @param val The unsigned integer value to format.
 * @param separator Character to use between hex bytes (default: '.').
 * @param show_length Whether to append the byte count in parentheses (default: true).
 * @return Formatted hex string with most significant byte first.
 *
 * @note The length will only be appended if show_length is true AND sizeof(T) is greater than 4.
 *
 * Example:
 * @code
 * uint32_t value = 0x12345678;
 * format_hex_pretty(value);        // Returns "12.34.56.78" (no length shown for <= 4 parts)
 * uint64_t value2 = 0x123456789ABCDEF0;
 * format_hex_pretty(value2);       // Returns "12.34.56.78.9A.BC.DE.F0 (8)"
 * format_hex_pretty(value2, ':');  // Returns "12:34:56:78:9A:BC:DE:F0 (8)"
 * format_hex_pretty<uint16_t>(0x1234); // Returns "12.34"
 * @endcode
 */
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
std::string format_hex_pretty(T val, char separator = '.', bool show_length = true) {
  val = convert_big_endian(val);
  return format_hex_pretty(reinterpret_cast<uint8_t *>(&val), sizeof(T), separator, show_length);
}

/// Format the byte array \p data of length \p len in binary.
std::string format_bin(const uint8_t *data, size_t length);
/// Format an unsigned integer in binary, starting with the most significant byte.
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

/// Create a string from a value and an accuracy in decimals.
std::string value_accuracy_to_string(float value, int8_t accuracy_decimals);
/// Create a string from a value, an accuracy in decimals, and a unit of measurement.
std::string value_accuracy_with_uom_to_string(float value, int8_t accuracy_decimals, StringRef unit_of_measurement);

/// Derive accuracy in decimals from an increment step.
int8_t step_to_accuracy_decimals(float step);

std::string base64_encode(const uint8_t *buf, size_t buf_len);
std::string base64_encode(const std::vector<uint8_t> &buf);

std::vector<uint8_t> base64_decode(const std::string &encoded_string);
size_t base64_decode(std::string const &encoded_string, uint8_t *buf, size_t buf_len);

///@}

/// @name Colors
///@{

/// Applies gamma correction of \p gamma to \p value.
float gamma_correct(float value, float gamma);
/// Reverts gamma correction of \p gamma to \p value.
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

template<typename... X> class CallbackManager;

/** Helper class to allow having multiple subscribers to a callback.
 *
 * @tparam Ts The arguments for the callbacks, wrapped in void().
 */
template<typename... Ts> class CallbackManager<void(Ts...)> {
 public:
  /// Add a callback to the list.
  void add(std::function<void(Ts...)> &&callback) { this->callbacks_.push_back(std::move(callback)); }

  /// Call all callbacks in this manager.
  void call(Ts... args) {
    for (auto &cb : this->callbacks_)
      cb(args...);
  }
  size_t size() const { return this->callbacks_.size(); }

  /// Call all callbacks in this manager.
  void operator()(Ts... args) { call(args...); }

 protected:
  std::vector<std::function<void(Ts...)>> callbacks_;
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
  Mutex();
  Mutex(const Mutex &) = delete;
  ~Mutex();
  void lock();
  bool try_lock();
  void unlock();

  Mutex &operator=(const Mutex &) = delete;

 private:
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  SemaphoreHandle_t handle_;
#else
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
 * This is needed on multi-threaded platforms (ESP32) when CONFIG_LWIP_TCPIP_CORE_LOCKING is enabled.
 * It ensures thread-safe access to lwIP APIs.
 *
 * @note This follows the same pattern as InterruptLock - platform-specific implementations in helpers.cpp
 */
class LwIPLock {
 public:
  LwIPLock();
  ~LwIPLock();

  // Delete copy constructor and copy assignment operator to prevent accidental copying
  LwIPLock(const LwIPLock &) = delete;
  LwIPLock &operator=(const LwIPLock &) = delete;
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
  static bool is_high_frequency();

 protected:
  bool started_{false};
  static uint8_t num_requests;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

/// Get the device MAC address as raw bytes, written into the provided byte array (6 bytes).
void get_mac_address_raw(uint8_t *mac);  // NOLINT(readability-non-const-parameter)

/// Buffer size for MAC address in lowercase hex notation (12 hex chars + null terminator)
constexpr size_t MAC_ADDRESS_BUFFER_SIZE = 13;

/// Buffer size for MAC address in colon-separated uppercase hex notation (17 chars + null terminator)
constexpr size_t MAC_ADDRESS_PRETTY_BUFFER_SIZE = 18;

/// Get the device MAC address as a string, in lowercase hex notation.
std::string get_mac_address();

/// Get the device MAC address as a string, in colon-separated uppercase hex notation.
std::string get_mac_address_pretty();

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
 * - perform external allocation falling back to main memory if SPI RAM is full or unavailable
 * - perform external allocation only
 * - perform internal allocation only
 */
template<class T> class RAMAllocator {
 public:
  using value_type = T;

  enum Flags {
    NONE = 0,                 // Perform external allocation and fall back to internal memory
    ALLOC_EXTERNAL = 1 << 0,  // Perform external allocation only.
    ALLOC_INTERNAL = 1 << 1,  // Perform internal allocation only.
    ALLOW_FAILURE = 1 << 2,   // Does nothing. Kept for compatibility.
  };

  RAMAllocator() = default;
  RAMAllocator(uint8_t flags) {
    // default is both external and internal
    flags &= ALLOC_INTERNAL | ALLOC_EXTERNAL;
    if (flags != 0)
      this->flags_ = flags;
  }
  template<class U> constexpr RAMAllocator(const RAMAllocator<U> &other) : flags_{other.flags_} {}

  T *allocate(size_t n) { return this->allocate(n, sizeof(T)); }

  T *allocate(size_t n, size_t manual_size) {
    size_t size = n * manual_size;
    T *ptr = nullptr;
#ifdef USE_ESP32
    if (this->flags_ & Flags::ALLOC_EXTERNAL) {
      ptr = static_cast<T *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (ptr == nullptr && this->flags_ & Flags::ALLOC_INTERNAL) {
      ptr = static_cast<T *>(heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
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
    if (this->flags_ & Flags::ALLOC_EXTERNAL) {
      ptr = static_cast<T *>(heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (ptr == nullptr && this->flags_ & Flags::ALLOC_INTERNAL) {
      ptr = static_cast<T *>(heap_caps_realloc(p, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
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
