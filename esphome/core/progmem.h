#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/core/hal.h"  // For PROGMEM definition

// Platform-agnostic macros for PROGMEM string handling
// On ESP8266/Arduino: Use Arduino's F() macro for PROGMEM strings
// On other platforms: Use plain strings (no PROGMEM)

#ifdef USE_ESP8266
// ESP8266 uses Arduino macros
#define ESPHOME_F(string_literal) F(string_literal)
#define ESPHOME_PGM_P PGM_P
#define ESPHOME_PSTR(s) PSTR(s)
#define ESPHOME_strncpy_P strncpy_P
#define ESPHOME_strncat_P strncat_P
#define ESPHOME_snprintf_P snprintf_P
#define ESPHOME_strcmp_P strcmp_P
#define ESPHOME_strcasecmp_P strcasecmp_P
#define ESPHOME_strncmp_P strncmp_P
#define ESPHOME_strncasecmp_P strncasecmp_P
// Type for pointers to PROGMEM strings (for use with ESPHOME_F return values)
using ProgmemStr = const __FlashStringHelper *;
#else
#define ESPHOME_F(string_literal) (string_literal)
#define ESPHOME_PGM_P const char *
#define ESPHOME_PSTR(s) (s)
#define ESPHOME_strncpy_P strncpy
#define ESPHOME_strncat_P strncat
#define ESPHOME_snprintf_P snprintf
#define ESPHOME_strcmp_P strcmp
#define ESPHOME_strcasecmp_P strcasecmp
#define ESPHOME_strncmp_P strncmp
#define ESPHOME_strncasecmp_P strncasecmp
// Type for pointers to strings (no PROGMEM on non-ESP8266 platforms)
using ProgmemStr = const char *;
#endif

namespace esphome {

/// Helper for C++20 string literal template arguments
template<size_t N> struct FixedString {
  char data[N]{};
  constexpr FixedString(const char (&str)[N]) {
    for (size_t i = 0; i < N; ++i)
      data[i] = str[i];
  }
  constexpr size_t size() const { return N - 1; }  // exclude null terminator
};

/// Compile-time string table that packs strings into a single blob with offset lookup.
/// Use PROGMEM_STRING_TABLE macro to instantiate with proper flash placement on ESP8266.
///
/// Example:
///   PROGMEM_STRING_TABLE(MyStrings, "foo", "bar", "baz");
///   ProgmemStr str = MyStrings::get_progmem_str(idx, MyStrings::LAST_INDEX);  // For ArduinoJson
///   const LogString *log_str = MyStrings::get_log_str(idx, MyStrings::LAST_INDEX);  // For logging
///
template<FixedString... Strs> struct ProgmemStringTable {
  static constexpr size_t COUNT = sizeof...(Strs);
  static constexpr size_t BLOB_SIZE = (0 + ... + (Strs.size() + 1));

  /// Generate packed string blob at compile time
  static constexpr auto make_blob() {
    std::array<char, BLOB_SIZE> result{};
    size_t pos = 0;
    auto copy = [&](const auto &str) {
      for (size_t i = 0; i <= str.size(); ++i)
        result[pos++] = str.data[i];
    };
    (copy(Strs), ...);
    return result;
  }

  /// Generate offset table at compile time (uint8_t limits blob to 255 bytes)
  static constexpr auto make_offsets() {
    static_assert(COUNT > 0, "PROGMEM_STRING_TABLE must contain at least one string");
    static_assert(COUNT <= 255, "PROGMEM_STRING_TABLE supports at most 255 strings with uint8_t indices");
    static_assert(BLOB_SIZE <= 255, "PROGMEM_STRING_TABLE blob exceeds 255 bytes; use fewer/shorter strings");
    std::array<uint8_t, COUNT> result{};
    size_t pos = 0, idx = 0;
    ((result[idx++] = static_cast<uint8_t>(pos), pos += Strs.size() + 1), ...);
    return result;
  }
};

// Forward declaration for LogString (defined in log.h)
struct LogString;

/// Instantiate a ProgmemStringTable with PROGMEM storage.
/// Creates: Name::get_progmem_str(idx, fallback), Name::get_log_str(idx, fallback)
/// If idx >= COUNT, returns string at fallback. Use LAST_INDEX for common patterns.
#define PROGMEM_STRING_TABLE(Name, ...) \
  struct Name { \
    using Table = ::esphome::ProgmemStringTable<__VA_ARGS__>; \
    static constexpr size_t COUNT = Table::COUNT; \
    static constexpr uint8_t LAST_INDEX = COUNT - 1; \
    static constexpr size_t BLOB_SIZE = Table::BLOB_SIZE; \
    static constexpr auto BLOB PROGMEM = Table::make_blob(); \
    static constexpr auto OFFSETS PROGMEM = Table::make_offsets(); \
    static const char *get_(uint8_t idx, uint8_t fallback) { \
      if (idx >= COUNT) \
        idx = fallback; \
      return &BLOB[::esphome::progmem_read_byte(&OFFSETS[idx])]; \
    } \
    static ::ProgmemStr get_progmem_str(uint8_t idx, uint8_t fallback) { \
      return reinterpret_cast<::ProgmemStr>(get_(idx, fallback)); \
    } \
    static const ::esphome::LogString *get_log_str(uint8_t idx, uint8_t fallback) { \
      return reinterpret_cast<const ::esphome::LogString *>(get_(idx, fallback)); \
    } \
  }

}  // namespace esphome
