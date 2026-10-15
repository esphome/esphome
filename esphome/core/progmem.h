#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>

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
#define ESPHOME_strlen_P strlen_P
// Type for pointers to PROGMEM strings (for use with ESPHOME_F return values)
using ProgmemStr = const __FlashStringHelper *;
// Storage class for PROGMEM_STRING_TABLE data; LOG_STR_ARG treats a LogString as PGM_P
// on ESP8266, so the table data must be in flash to match.
#define ESPHOME_PROGMEM_STRING_TABLE_STORAGE PROGMEM
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
#define ESPHOME_strlen_P strlen
// Type for pointers to strings (no PROGMEM on non-ESP8266 platforms)
using ProgmemStr = const char *;
// No-op on non-ESP8266 platforms where PROGMEM itself is a no-op.
#define ESPHOME_PROGMEM_STRING_TABLE_STORAGE
#endif

namespace esphome {

/// Copies a string stored with ESPHOME_F into a std::string.
#ifdef USE_ESP8266
std::string progmem_string(ProgmemStr str);
#else
inline std::string progmem_string(ProgmemStr str) { return std::string(str); }
#endif

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
/// Blobs over 255 bytes have no offsets and are scanned, so keep those for rarely read strings.
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
  /// Length of the longest string, for sizing a RAM copy on ESP8266
  static constexpr size_t MAX_LENGTH = std::max({Strs.size()...});
  /// One-byte offsets address up to 255 bytes; a larger blob has no offset table and is scanned instead,
  /// which costs less flash than wider offsets for the rarely read tables that get this big.
  static constexpr bool INDEXED = BLOB_SIZE <= UINT8_MAX;

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

  /// Generate offset table at compile time
  static constexpr auto make_offsets() {
    static_assert(COUNT > 0, "PROGMEM_STRING_TABLE must contain at least one string");
    static_assert(COUNT <= 255, "PROGMEM_STRING_TABLE supports at most 255 strings with uint8_t indices");
    if constexpr (INDEXED) {
      std::array<uint8_t, COUNT> result{};
      size_t pos = 0, idx = 0;
      ((result[idx++] = static_cast<uint8_t>(pos), pos += Strs.size() + 1), ...);
      return result;
    } else {
      return std::array<uint8_t, 0>{};
    }
  }
};

/// Skip idx strings in a packed NUL-separated blob that may live in flash.
inline const char *progmem_skip_strings(const char *blob, uint8_t idx) {
  while (idx-- > 0)
    blob += ESPHOME_strlen_P(blob) + 1;
  return blob;
}

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
    static constexpr size_t MAX_LENGTH = Table::MAX_LENGTH; \
    static constexpr auto BLOB ESPHOME_PROGMEM_STRING_TABLE_STORAGE = Table::make_blob(); \
    static constexpr auto OFFSETS ESPHOME_PROGMEM_STRING_TABLE_STORAGE = Table::make_offsets(); \
    static const char *get_(uint8_t idx, uint8_t fallback) { \
      if (idx >= COUNT) \
        idx = fallback; \
      /* std::launder is used here to prevent the inter-procedural analysis that */ \
      /* causes the false positive that the string is not null terminated */ \
      if constexpr (Table::INDEXED) \
        return std::launder(&BLOB[::esphome::progmem_read_byte(&OFFSETS[idx])]); \
      else \
        return std::launder(::esphome::progmem_skip_strings(BLOB.data(), idx)); \
    } \
    static ::ProgmemStr get_progmem_str(uint8_t idx, uint8_t fallback) { \
      return reinterpret_cast<::ProgmemStr>(get_(idx, fallback)); \
    } \
    static const ::esphome::LogString *get_log_str(uint8_t idx, uint8_t fallback) { \
      return reinterpret_cast<const ::esphome::LogString *>(get_(idx, fallback)); \
    } \
  }

/// The index of the string value (len bytes, in RAM) in a PROGMEM_STRING_TABLE, or -1 when it is
/// not one of its strings. One pass over the blob, so a table without offsets costs the same.
template<typename Table> int16_t progmem_string_table_find(const char *value, size_t len) {
  const char *entry = Table::BLOB.data();
  for (uint8_t i = 0; i < Table::COUNT; i++) {
    const size_t entry_len = ESPHOME_strlen_P(entry);
    if (entry_len == len && ESPHOME_strncmp_P(value, entry, len) == 0) {
      return i;
    }
    entry += entry_len + 1;
  }
  return -1;
}

/// A PROGMEM_STRING_TABLE copied onto the stack, so its strings are plain pointers that live as long
/// as the object
template<typename Name> class ProgmemStringTableStackCopy {
  static_assert(Name::Table::INDEXED, "the table needs offsets");

 public:
  ProgmemStringTableStackCopy() { progmem_memcpy(this->blob_, Name::BLOB.data(), Name::BLOB_SIZE); }
  const char *get(uint8_t idx) const { return this->blob_ + progmem_read_byte(&Name::OFFSETS[idx]); }

 private:
  char blob_[Name::BLOB_SIZE];
};

/// A PROGMEM_STRING_TABLE read in place, with the interface of ProgmemStringTableStackCopy
template<typename Name> class ProgmemStringTableView {
  static_assert(Name::Table::INDEXED, "the table needs offsets");

 public:
  const char *get(uint8_t idx) const { return Name::get_(idx, idx); }
};

/// The strings of a PROGMEM_STRING_TABLE addressable as plain pointers, for code that hands them on
/// without reading them itself, such as an API request. On the ESP8266 this is a copy of the table on
/// the stack; elsewhere the table is read in place.
///
/// Example, with an enum that numbers the table's strings:
///   ProgmemStringTableCopy<MyStrings> strings;
///   request.service = StringRef(strings.get(MY_STRING_TURN_ON));
#ifdef USE_ESP8266
template<typename Name> using ProgmemStringTableCopy = ProgmemStringTableStackCopy<Name>;
#else
template<typename Name> using ProgmemStringTableCopy = ProgmemStringTableView<Name>;
#endif

}  // namespace esphome
