#pragma once

#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "esphome/core/optional.h"

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

#define HOT __attribute__((hot))
#define ESPDEPRECATED(msg, when) __attribute__((deprecated(msg)))
#define ESPHOME_ALWAYS_INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

// Various functions can be constexpr in C++14, but not in C++11 (because their body isn't just a return statement).
// Define a substitute constexpr keyword for those functions, until we can drop C++11 support.
#if __cplusplus >= 201402L
#define constexpr14 constexpr
#else
#define constexpr14 inline  // constexpr implies inline
#endif

namespace esphome {

/// @name STL backports
///@{

// Backports for various STL features we like to use. Pull in the STL implementation wherever available, to avoid
// ambiguity and to provide a uniform API.

// std::to_string() from C++11, available from libstdc++/g++ 8
// See https://github.com/espressif/esp-idf/issues/1445
#if _GLIBCXX_RELEASE >= 8
using std::to_string;
#else
std::string to_string(int value);                 // NOLINT
std::string to_string(long value);                // NOLINT
std::string to_string(long long value);           // NOLINT
std::string to_string(unsigned value);            // NOLINT
std::string to_string(unsigned long value);       // NOLINT
std::string to_string(unsigned long long value);  // NOLINT
std::string to_string(float value);
std::string to_string(double value);
std::string to_string(long double value);
#endif

// std::is_trivially_copyable from C++11, implemented in libstdc++/g++ 5.1 (but minor releases can't be detected)
#if _GLIBCXX_RELEASE >= 6
using std::is_trivially_copyable;
#else
// Implementing this is impossible without compiler intrinsics, so don't bother. Invalid usage will be detected on
// other variants that use a newer compiler anyway.
// NOLINTNEXTLINE(readability-identifier-naming)
template<typename T> struct is_trivially_copyable : public std::integral_constant<bool, true> {};
#endif

// std::make_unique() from C++14
#if __cpp_lib_make_unique >= 201304
using std::make_unique;
#else
template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args &&...args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif

// std::enable_if_t from C++14
#if __cplusplus >= 201402L
using std::enable_if_t;
#else
template<bool B, class T = void> using enable_if_t = typename std::enable_if<B, T>::type;
#endif

// std::clamp from C++17
#if __cpp_lib_clamp >= 201603
using std::clamp;
#else
template<typename T, typename Compare> constexpr const T &clamp(const T &v, const T &lo, const T &hi, Compare comp) {
  return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}
template<typename T> constexpr const T &clamp(const T &v, const T &lo, const T &hi) {
  return clamp(v, lo, hi, std::less<T>{});
}
#endif

// std::is_invocable from C++17
#if __cpp_lib_is_invocable >= 201703
using std::is_invocable;
#else
// https://stackoverflow.com/a/37161919/8924614
template<class T, class... Args> struct is_invocable {  // NOLINT(readability-identifier-naming)
  template<class U> static auto test(U *p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
  template<class U> static auto test(...) -> decltype(std::false_type());
  static constexpr auto value = decltype(test<T>(nullptr))::value;  // NOLINT
};
#endif

// std::bit_cast from C++20
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

// std::byteswap from C++23
template<typename T> constexpr14 T byteswap(T n) {
  T m;
  for (size_t i = 0; i < sizeof(T); i++)
    reinterpret_cast<uint8_t *>(&m)[i] = reinterpret_cast<uint8_t *>(&n)[sizeof(T) - 1 - i];
  return m;
}
template<> constexpr14 uint8_t byteswap(uint8_t n) { return n; }
template<> constexpr14 uint16_t byteswap(uint16_t n) { return __builtin_bswap16(n); }
template<> constexpr14 uint32_t byteswap(uint32_t n) { return __builtin_bswap32(n); }
template<> constexpr14 uint64_t byteswap(uint64_t n) { return __builtin_bswap64(n); }
template<> constexpr14 int8_t byteswap(int8_t n) { return n; }
template<> constexpr14 int16_t byteswap(int16_t n) { return __builtin_bswap16(n); }
template<> constexpr14 int32_t byteswap(int32_t n) { return __builtin_bswap32(n); }
template<> constexpr14 int64_t byteswap(int64_t n) { return __builtin_bswap64(n); }

///@}

/// @name Mathematics
///@{

/// Linearly interpolate between \p start and \p end by \p completion (between 0 and 1).
float lerp(float completion, float start, float end);

/// Remap \p value from the range (\p min, \p max) to (\p min_out, \p max_out).
template<typename T, typename U> T remap(U value, U min, U max, T min_out, T max_out) {
  return (value - min) * (max_out - min_out) / (max - min) + min_out;
}

/// Calculate a CRC-8 checksum of \p data with size \p len.
uint8_t crc8(const uint8_t *data, uint8_t len);

/// Calculate a CRC-16 checksum of \p data with size \p len.
uint16_t crc16(const uint8_t *data, uint16_t len, uint16_t crc = 0xffff, uint16_t reverse_poly = 0xa001,
               bool refin = false, bool refout = false);
uint16_t crc16be(const uint8_t *data, uint16_t len, uint16_t crc = 0, uint16_t poly = 0x1021, bool refin = false,
                 bool refout = false);

/// Calculate a FNV-1 hash of \p str.
uint32_t fnv1_hash(const std::string &str);

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
/// Encode a 32-bit value given four bytes in most to least significant byte order.
constexpr uint32_t encode_uint32(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
  return (static_cast<uint32_t>(byte1) << 24) | (static_cast<uint32_t>(byte2) << 16) |
         (static_cast<uint32_t>(byte3) << 8) | (static_cast<uint32_t>(byte4));
}
/// Encode a 24-bit value given three bytes in most to least significant byte order.
constexpr uint32_t encode_uint24(uint8_t byte1, uint8_t byte2, uint8_t byte3) {
  return ((static_cast<uint32_t>(byte1) << 16) | (static_cast<uint32_t>(byte2) << 8) | (static_cast<uint32_t>(byte3)));
}

/// Encode a value from its constituent bytes (from most to least significant) in an array with length sizeof(T).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
constexpr14 T encode_value(const uint8_t *bytes) {
  T val = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    val <<= 8;
    val |= bytes[i];
  }
  return val;
}
/// Encode a value from its constituent bytes (from most to least significant) in an std::array with length sizeof(T).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
constexpr14 T encode_value(const std::array<uint8_t, sizeof(T)> bytes) {
  return encode_value<T>(bytes.data());
}
/// Decode a value into its constituent bytes (from most to least significant).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
constexpr14 std::array<uint8_t, sizeof(T)> decode_value(T val) {
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
template<typename T> constexpr14 T convert_big_endian(T val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return byteswap(val);
#else
  return val;
#endif
}

/// Convert a value between host byte order and little endian (least significant byte first) order.
template<typename T> constexpr14 T convert_little_endian(T val) {
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

/// Convert the value to a string (added as extra overload so that to_string() can be used on all stringifiable types).
inline std::string to_string(const std::string &val) { return val; }

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

/// Format the byte array \p data of length \p len in pretty-printed, human-readable hex.
std::string format_hex_pretty(const uint8_t *data, size_t length);
/// Format the word array \p data of length \p len in pretty-printed, human-readable hex.
std::string format_hex_pretty(const uint16_t *data, size_t length);
/// Format the vector \p data in pretty-printed, human-readable hex.
std::string format_hex_pretty(const std::vector<uint8_t> &data);
/// Format the vector \p data in pretty-printed, human-readable hex.
std::string format_hex_pretty(const std::vector<uint16_t> &data);
/// Format an unsigned integer in pretty-printed, human-readable hex, starting with the most significant byte.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> std::string format_hex_pretty(T val) {
  val = convert_big_endian(val);
  return format_hex_pretty(reinterpret_cast<uint8_t *>(&val), sizeof(T));
}

/// Return values for parse_on_off().
enum ParseOnOffState {
  PARSE_NONE = 0,
  PARSE_ON,
  PARSE_OFF,
  PARSE_TOGGLE,
};
/// Parse a string that contains either on, off or toggle.
ParseOnOffState parse_on_off(const char *str, const char *on = nullptr, const char *off = nullptr);

/// Create a string from a value and an accuracy in decimals.
std::string value_accuracy_to_string(float value, int8_t accuracy_decimals);

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
  /// Feeds the next item in the series to the deduplicator and returns whether this is a duplicate.
  bool next(T value) {
    if (this->has_value_) {
      if (this->last_value_ == value)
        return false;
    }
    this->has_value_ = true;
    this->last_value_ = value;
    return true;
  }
  /// Returns whether this deduplicator has processed any items so far.
  bool has_value() const { return this->has_value_; }

 protected:
  bool has_value_{false};
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
  void lock();
  bool try_lock();
  void unlock();

  Mutex &operator=(const Mutex &) = delete;

 private:
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  SemaphoreHandle_t handle_;
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
#if defined(USE_ESP8266) || defined(USE_RP2040)
  uint32_t state_;
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
  static bool is_high_frequency();

 protected:
  bool started_{false};
  static uint8_t num_requests;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

/// Get the device MAC address as raw bytes, written into the provided byte array (6 bytes).
void get_mac_address_raw(uint8_t *mac);  // NOLINT(readability-non-const-parameter)

/// Get the device MAC address as a string, in lowercase hex notation.
std::string get_mac_address();

/// Get the device MAC address as a string, in colon-separated uppercase hex notation.
std::string get_mac_address_pretty();

#ifdef USE_ESP32
/// Set the MAC address to use from the provided byte array (6 bytes).
void set_mac_address(uint8_t *mac);
#endif

/// Delay for the given amount of microseconds, possibly yielding to other processes during the wait.
void delay_microseconds_safe(uint32_t us);

///@}

/// @name Memory management
///@{

/** An STL allocator that uses SPI RAM.
 *
 * By setting flags, it can be configured to don't try main memory if SPI RAM is full or unavailable, and to return
 * `nulllptr` instead of aborting when no memory is available.
 */
template<class T> class ExternalRAMAllocator {
 public:
  using value_type = T;

  enum Flags {
    NONE = 0,
    REFUSE_INTERNAL = 1 << 0,  ///< Refuse falling back to internal memory when external RAM is full or unavailable.
    ALLOW_FAILURE = 1 << 1,    ///< Don't abort when memory allocation fails.
  };

  ExternalRAMAllocator() = default;
  ExternalRAMAllocator(Flags flags) : flags_{flags} {}
  template<class U> constexpr ExternalRAMAllocator(const ExternalRAMAllocator<U> &other) : flags_{other.flags_} {}

  T *allocate(size_t n) {
    size_t size = n * sizeof(T);
    T *ptr = nullptr;
#ifdef USE_ESP32
    ptr = static_cast<T *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif
    if (ptr == nullptr && (this->flags_ & Flags::REFUSE_INTERNAL) == 0)
      ptr = static_cast<T *>(malloc(size));  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
    if (ptr == nullptr && (this->flags_ & Flags::ALLOW_FAILURE) == 0)
      abort();
    return ptr;
  }

  void deallocate(T *p, size_t n) {
    free(p);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  }

 private:
  Flags flags_{Flags::ALLOW_FAILURE};
};

/// @}

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

/// @name Deprecated functions
///@{

ESPDEPRECATED("hexencode() is deprecated, use format_hex_pretty() instead.", "2022.1")
inline std::string hexencode(const uint8_t *data, uint32_t len) { return format_hex_pretty(data, len); }

template<typename T>
ESPDEPRECATED("hexencode() is deprecated, use format_hex_pretty() instead.", "2022.1")
std::string hexencode(const T &data) {
  return hexencode(data.data(), data.size());
}

///@}

}  // namespace esphome
