#pragma once

#include <cmath>

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <type_traits>

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "esp32-hal-psram.h"
#endif

#include "esphome/core/optional.h"

#define HOT __attribute__((hot))
#define ESPDEPRECATED(msg, when) __attribute__((deprecated(msg)))
#define ALWAYS_INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

#define xSemaphoreWait(semaphore, wait_time) \
  xSemaphoreTake(semaphore, wait_time); \
  xSemaphoreGive(semaphore);

namespace esphome {

/// Get the device MAC address as raw bytes, written into the provided byte array (6 bytes).
void get_mac_address_raw(uint8_t *mac);

/// Get the device MAC address as a string, in lowercase hex notation.
std::string get_mac_address();

/// Get the device MAC address as a string, in colon-separated uppercase hex notation.
std::string get_mac_address_pretty();

#ifdef USE_ESP32
/// Set the MAC address to use from the provided byte array (6 bytes).
void set_mac_address(uint8_t *mac);
#endif

std::string to_string(const std::string &val);
std::string to_string(int val);
std::string to_string(long val);                // NOLINT
std::string to_string(long long val);           // NOLINT
std::string to_string(unsigned val);            // NOLINT
std::string to_string(unsigned long val);       // NOLINT
std::string to_string(unsigned long long val);  // NOLINT
std::string to_string(float val);
std::string to_string(double val);
std::string to_string(long double val);
optional<int> parse_hex(const std::string &str, size_t start, size_t length);
optional<int> parse_hex(char chr);

/// Compare string a to string b (ignoring case) and return whether they are equal.
bool str_equals_case_insensitive(const std::string &a, const std::string &b);
bool str_startswith(const std::string &full, const std::string &start);
bool str_endswith(const std::string &full, const std::string &ending);

/// snprintf-like function returning std::string with a given maximum length.
std::string __attribute__((format(printf, 1, 3))) str_snprintf(const char *fmt, size_t length, ...);

/// sprintf-like function returning std::string.
std::string __attribute__((format(printf, 1, 2))) str_sprintf(const char *fmt, ...);

class HighFrequencyLoopRequester {
 public:
  void start();
  void stop();

  static bool is_high_frequency();

 protected:
  bool started_{false};
};

/** Clamp the value between min and max.
 *
 * @param val The value.
 * @param min The minimum value.
 * @param max The maximum value.
 * @return val clamped in between min and max.
 */
template<typename T> T clamp(T val, T min, T max);

/** Linearly interpolate between end start and end by completion.
 *
 * @tparam T The input/output typename.
 * @param start The start value.
 * @param end The end value.
 * @param completion The completion. 0 is start value, 1 is end value.
 * @return The linearly interpolated value.
 */
float lerp(float completion, float start, float end);

// Not all platforms we support target C++14 yet, so we can't unconditionally use std::make_unique. Provide our own
// implementation if needed, and otherwise pull std::make_unique into scope so that we have a uniform API.
#if __cplusplus >= 201402L
using std::make_unique;
#else
template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args &&...args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif

/// Return a random 32 bit unsigned integer.
uint32_t random_uint32();

/** Returns a random double between 0 and 1.
 *
 * Note: This function probably doesn't provide a truly uniform distribution.
 */
double random_double();

/// Returns a random float between 0 and 1. Essentially just casts random_double() to a float.
float random_float();

void fill_random(uint8_t *data, size_t len);

void fast_random_set_seed(uint32_t seed);
uint32_t fast_random_32();
uint16_t fast_random_16();
uint8_t fast_random_8();

/// Applies gamma correction with the provided gamma to value.
float gamma_correct(float value, float gamma);
/// Reverts gamma correction with the provided gamma to value.
float gamma_uncorrect(float value, float gamma);

/// Create a string from a value and an accuracy in decimals.
std::string value_accuracy_to_string(float value, int8_t accuracy_decimals);

/// Convert a uint64_t to a hex string
std::string uint64_to_string(uint64_t num);

/// Convert a uint32_t to a hex string
std::string uint32_to_string(uint32_t num);

uint8_t reverse_bits_8(uint8_t x);
uint16_t reverse_bits_16(uint16_t x);
uint32_t reverse_bits_32(uint32_t x);

/// Convert RGB floats (0-1) to hue (0-360) & saturation/value percentage (0-1)
void rgb_to_hsv(float red, float green, float blue, int &hue, float &saturation, float &value);
/// Convert hue (0-360) & saturation/value percentage (0-1) to RGB floats (0-1)
void hsv_to_rgb(int hue, float saturation, float value, float &red, float &green, float &blue);

/***
 * An interrupt helper class.
 *
 * This behaves like std::lock_guard. As long as the value is visible in the current stack, all interrupts
 * (including flash reads) will be disabled.
 *
 * Please note all functions called when the interrupt lock must be marked IRAM_ATTR (loading code into
 * instruction cache is done via interrupts; disabling interrupts prevents data not already in cache from being
 * pulled from flash).
 *
 * Example:
 *
 * ```cpp
 * // interrupts are enabled
 * {
 *   InterruptLock lock;
 *   // do something
 *   // interrupts are disabled
 * }
 * // interrupts are enabled
 * ```
 */
class InterruptLock {
 public:
  InterruptLock();
  ~InterruptLock();

 protected:
#ifdef USE_ESP8266
  uint32_t xt_state_;
#endif
};

/// Calculate a crc8 of data with the provided data length.
uint8_t crc8(uint8_t *data, uint8_t len);

enum ParseOnOffState {
  PARSE_NONE = 0,
  PARSE_ON,
  PARSE_OFF,
  PARSE_TOGGLE,
};

ParseOnOffState parse_on_off(const char *str, const char *on = nullptr, const char *off = nullptr);

// Encode raw data to a human-readable string (for debugging)
std::string hexencode(const uint8_t *data, uint32_t len);
template<typename T> std::string hexencode(const T &data) { return hexencode(data.data(), data.size()); }

// https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer/7858971#7858971
template<int...> struct seq {};                                       // NOLINT
template<int N, int... S> struct gens : gens<N - 1, N - 1, S...> {};  // NOLINT
template<int... S> struct gens<0, S...> { using type = seq<S...>; };  // NOLINT

template<bool B, class T = void> using enable_if_t = typename std::enable_if<B, T>::type;

template<typename T, enable_if_t<!std::is_pointer<T>::value, int> = 0> T id(T value) { return value; }
template<typename T, enable_if_t<std::is_pointer<T *>::value, int> = 0> T &id(T *value) { return *value; }

template<typename... X> class CallbackManager;

/** Simple helper class to allow having multiple subscribers to a signal.
 *
 * @tparam Ts The arguments for the callback, wrapped in void().
 */
template<typename... Ts> class CallbackManager<void(Ts...)> {
 public:
  /// Add a callback to the internal callback list.
  void add(std::function<void(Ts...)> &&callback) { this->callbacks_.push_back(std::move(callback)); }

  /// Call all callbacks in this manager.
  void call(Ts... args) {
    for (auto &cb : this->callbacks_)
      cb(args...);
  }

 protected:
  std::vector<std::function<void(Ts...)>> callbacks_;
};

// https://stackoverflow.com/a/37161919/8924614
template<class T, class... Args>
struct is_callable  // NOLINT
{
  template<class U> static auto test(U *p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());

  template<class U> static auto test(...) -> decltype(std::false_type());

  static constexpr auto value = decltype(test<T>(nullptr))::value;  // NOLINT
};

void delay_microseconds_safe(uint32_t us);

template<typename T> class Deduplicator {
 public:
  bool next(T value) {
    if (this->has_value_) {
      if (this->last_value_ == value)
        return false;
    }
    this->has_value_ = true;
    this->last_value_ = value;
    return true;
  }
  bool has_value() const { return this->has_value_; }

 protected:
  bool has_value_{false};
  T last_value_{};
};

template<typename T> class Parented {
 public:
  Parented() {}
  Parented(T *parent) : parent_(parent) {}

  T *get_parent() const { return parent_; }
  void set_parent(T *parent) { parent_ = parent; }

 protected:
  T *parent_{nullptr};
};

uint32_t fnv1_hash(const std::string &str);

template<typename T> T *new_buffer(size_t length) {
  T *buffer;
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  if (psramFound()) {
    buffer = (T *) ps_malloc(length);
  } else {
    buffer = new T[length];  // NOLINT(cppcoreguidelines-owning-memory)
  }
#else
  buffer = new T[length];  // NOLINT(cppcoreguidelines-owning-memory)
#endif

  return buffer;
}

// ---------------------------------------------------------------------------------------------------------------------

/// @name STL backports
///@{

// std::byteswap is from C++23 and technically should be a template, but this will do for now.
constexpr uint8_t byteswap(uint8_t n) { return n; }
constexpr uint16_t byteswap(uint16_t n) { return __builtin_bswap16(n); }
constexpr uint32_t byteswap(uint32_t n) { return __builtin_bswap32(n); }
constexpr uint64_t byteswap(uint64_t n) { return __builtin_bswap64(n); }

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

/// Encode a value from its constituent bytes (from most to least significant) in an array with length sizeof(T).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> inline T encode_value(const uint8_t *bytes) {
  T val = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    val <<= 8;
    val |= bytes[i];
  }
  return val;
}
/// Encode a value from its constituent bytes (from most to least significant) in an std::array with length sizeof(T).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
inline T encode_value(const std::array<uint8_t, sizeof(T)> bytes) {
  return encode_value<T>(bytes.data());
}
/// Decode a value into its constituent bytes (from most to least significant).
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
inline std::array<uint8_t, sizeof(T)> decode_value(T val) {
  std::array<uint8_t, sizeof(T)> ret{};
  for (size_t i = sizeof(T); i > 0; i--) {
    ret[i - 1] = val & 0xFF;
    val >>= 8;
  }
  return ret;
}

/// Convert a value between host byte order and big endian (most significant byte first) order.
template<typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0> constexpr T convert_big_endian(T val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return byteswap(val);
#else
  return val;
#endif
}

///@}

/// @name Strings
///@{

/// Truncate a string to a specific length.
std::string str_truncate(const std::string &str, size_t length);

/// Extract the part of the string until either the first occurence of the specified character, or the end (requires str
/// to be null-terminated).
std::string str_until(const char *str, char ch);
/// Extract the part of the string until either the first occurence of the specified character, or the end.
std::string str_until(const std::string &str, char ch);

/// Convert the string to snake case (lowercase with underscores).
std::string str_snake_case(const std::string &str);

/// Sanitizes the input string by removing all characters but alphanumerics, dashes and underscores.
std::string str_sanitize(const std::string &str);

///@}

/// @name Parsing & formatting
///@{

/// Parse an unsigned decimal number (requires null-terminated string).
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_unsigned<T>::value), int> = 0>
optional<T> parse_number(const char *str, size_t len) {
  char *end = nullptr;
  unsigned long value = ::strtoul(str, &end, 10);  // NOLINT(google-runtime-int)
  if (end == str || *end != '\0' || value > std::numeric_limits<T>::max())
    return {};
  return value;
}
/// Parse an unsigned decimal number.
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_unsigned<T>::value), int> = 0>
optional<T> parse_number(const std::string &str) {
  return parse_number<T>(str.c_str(), str.length() + 1);
}
/// Parse a signed decimal number (requires null-terminated string).
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_signed<T>::value), int> = 0>
optional<T> parse_number(const char *str, size_t len) {
  char *end = nullptr;
  signed long value = ::strtol(str, &end, 10);  // NOLINT(google-runtime-int)
  if (end == str || *end != '\0' || value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
    return {};
  return value;
}
/// Parse a signed decimal number.
template<typename T, enable_if_t<(std::is_integral<T>::value && std::is_signed<T>::value), int> = 0>
optional<T> parse_number(const std::string &str) {
  return parse_number<T>(str.c_str(), str.length() + 1);
}
/// Parse a decimal floating-point number (requires null-terminated string).
template<typename T, enable_if_t<(std::is_same<T, float>::value), int> = 0>
optional<T> parse_number(const char *str, size_t len) {
  char *end = nullptr;
  float value = ::strtof(str, &end);
  if (end == str || *end != '\0' || value == HUGE_VALF)
    return {};
  return value;
}
/// Parse a decimal floating-point number.
template<typename T, enable_if_t<(std::is_same<T, float>::value), int> = 0>
optional<T> parse_number(const std::string &str) {
  return parse_number<T>(str.c_str(), str.length() + 1);
}

///@}

}  // namespace esphome
