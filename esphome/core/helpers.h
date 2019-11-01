#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <type_traits>

#include "esphome/core/optional.h"
#include "esphome/core/esphal.h"

#ifdef CLANG_TIDY
#undef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR
#undef ICACHE_RODATA_ATTR
#define ICACHE_RODATA_ATTR
#endif

#define HOT __attribute__((hot))
#define ESPDEPRECATED(msg) __attribute__((deprecated(msg)))
#define ALWAYS_INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

namespace esphome {

/// The characters that are allowed in a hostname.
extern const char *HOSTNAME_CHARACTER_WHITELIST;

/// Gets the MAC address as a string, this can be used as way to identify this ESP.
std::string get_mac_address();

std::string get_mac_address_pretty();

std::string to_string(const std::string &val);
std::string to_string(int val);
std::string to_string(long val);
std::string to_string(long long val);
std::string to_string(unsigned val);
std::string to_string(unsigned long val);
std::string to_string(unsigned long long val);
std::string to_string(float val);
std::string to_string(double val);
std::string to_string(long double val);
optional<float> parse_float(const std::string &str);

/// Sanitize the hostname by removing characters that are not in the whitelist and truncating it to 63 chars.
std::string sanitize_hostname(const std::string &hostname);

/// Truncate a string to a specific length
std::string truncate_string(const std::string &s, size_t length);

/// Convert the string to lowercase_underscore.
std::string to_lowercase_underscore(std::string s);

/// Compare string a to string b (ignoring case) and return whether they are equal.
bool str_equals_case_insensitive(const std::string &a, const std::string &b);
bool str_startswith(const std::string &full, const std::string &start);
bool str_endswith(const std::string &full, const std::string &ending);

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
float clamp(float val, float min, float max);

/** Linearly interpolate between end start and end by completion.
 *
 * @tparam T The input/output typename.
 * @param start The start value.
 * @param end The end value.
 * @param completion The completion. 0 is start value, 1 is end value.
 * @return The linearly interpolated value.
 */
float lerp(float completion, float start, float end);

/// std::make_unique
template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args &&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

/// Return a random 32 bit unsigned integer.
uint32_t random_uint32();

/** Returns a random double between 0 and 1.
 *
 * Note: This function probably doesn't provide a truly uniform distribution.
 */
double random_double();

/// Returns a random float between 0 and 1. Essentially just casts random_double() to a float.
float random_float();

void fast_random_set_seed(uint32_t seed);
uint32_t fast_random_32();
uint16_t fast_random_16();
uint8_t fast_random_8();

/// Applies gamma correction with the provided gamma to value.
float gamma_correct(float value, float gamma);

/// Create a string from a value and an accuracy in decimals.
std::string value_accuracy_to_string(float value, int8_t accuracy_decimals);

/// Convert a uint64_t to a hex string
std::string uint64_to_string(uint64_t num);

/// Convert a uint32_t to a hex string
std::string uint32_to_string(uint32_t num);

/// Sanitizes the input string with the whitelist.
std::string sanitize_string_whitelist(const std::string &s, const std::string &whitelist);

uint8_t reverse_bits_8(uint8_t x);
uint16_t reverse_bits_16(uint16_t x);
uint32_t reverse_bits_32(uint32_t x);

/// Encode a 16-bit unsigned integer given a most and least-significant byte.
uint16_t encode_uint16(uint8_t msb, uint8_t lsb);
/// Decode a 16-bit unsigned integer into an array of two values: most significant byte, least significant byte.
std::array<uint8_t, 2> decode_uint16(uint16_t value);

/** Cross-platform method to disable interrupts.
 *
 * Useful when you need to do some timing-dependent communication.
 *
 * @see Do not forget to call `enable_interrupts()` again or otherwise things will go very wrong.
 */
void disable_interrupts();

/// Cross-platform method to enable interrupts after they have been disabled.
void enable_interrupts();

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

template<typename T, typename... X> class TemplatableValue {
 public:
  TemplatableValue() : type_(EMPTY) {}

  template<typename F, enable_if_t<!is_callable<F, X...>::value, int> = 0>
  TemplatableValue(F value) : type_(VALUE), value_(value) {}

  template<typename F, enable_if_t<is_callable<F, X...>::value, int> = 0>
  TemplatableValue(F f) : type_(LAMBDA), f_(f) {}

  bool has_value() { return this->type_ != EMPTY; }

  T value(X... x) {
    if (this->type_ == LAMBDA) {
      return this->f_(x...);
    }
    // return value also when empty
    return this->value_;
  }

  optional<T> optional_value(X... x) {
    if (!this->has_value()) {
      return {};
    }
    return this->value(x...);
  }

  T value_or(X... x, T default_value) {
    if (!this->has_value()) {
      return default_value;
    }
    return this->value(x...);
  }

 protected:
  enum {
    EMPTY,
    VALUE,
    LAMBDA,
  } type_;

  T value_;
  std::function<T(X...)> f_;
};

template<typename... X> class TemplatableStringValue : public TemplatableValue<std::string, X...> {
 public:
  TemplatableStringValue() : TemplatableValue<std::string, X...>() {}

  template<typename F, enable_if_t<!is_callable<F, X...>::value, int> = 0>
  TemplatableStringValue(F value) : TemplatableValue<std::string, X...>(value) {}

  template<typename F, enable_if_t<is_callable<F, X...>::value, int> = 0>
  TemplatableStringValue(F f)
      : TemplatableValue<std::string, X...>([f](X... x) -> std::string { return to_string(f(x...)); }) {}
};

void delay_microseconds_accurate(uint32_t usec);

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

}  // namespace esphome
