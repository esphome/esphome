/*
 * Linker wrap stub for sscanf on ESP-IDF, newlib only.
 *
 * Nothing in ESPHome calls sscanf, but bluedroid does in two places:
 * btc_config_get_bin decodes stored bonding keys with "%02x" and
 * string_to_bdaddr parses "%02x:%02x:%02x:%02x:%02x:%02x". Those two
 * calls are the only reference to newlib's scanf engine (__ssvfscanf_r,
 * _strtod_l and their helpers, ~13 KB) in a Bluetooth build on variants
 * whose ROM does not export sscanf.
 *
 * This stub replaces sscanf with a small scanner covering the integer,
 * character, string and scanset conversions, so any other caller in the
 * image (user lambdas included) keeps working. Floating-point conversions
 * are the one gap: supporting them would pull _strtod_l straight back, so
 * a format using %f and friends aborts with a message pointing at
 * enable_full_scanf: true. Codegen already keeps the libc sscanf when a
 * lambda scans a float.
 *
 * Only compiled in when codegen defines USE_ESP32_SSCANF_STUB, which is
 * gated on a Bluetooth component being present, on the variant's ROM and
 * on the same newlib condition as printf_stubs.cpp.
 */

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_SSCANF_STUB)

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "esp_system.h"
#include "esphome/core/helpers.h"

namespace esphome::esp32 {

namespace {

// Store width of an integer conversion. Every ESP32 target is ILP32, so the
// bare length, l, z and t all store 32 bits and ll and j store 64.
enum class SscanfLength : uint8_t {
  SSCANF_LENGTH_NONE,
  SSCANF_LENGTH_HH,
  SSCANF_LENGTH_H,
  SSCANF_LENGTH_LL,
};

bool is_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

// The bit pattern is what the signed conversions want as well. All object
// pointers share one representation here, so the argument is fetched once
// as void * rather than once per pointee type.
void store_int(va_list &ap, SscanfLength length, unsigned long long value) {
  void *dest = va_arg(ap, void *);
  switch (length) {
    case SscanfLength::SSCANF_LENGTH_HH:
      *static_cast<uint8_t *>(dest) = static_cast<uint8_t>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_H:
      *static_cast<uint16_t *>(dest) = static_cast<uint16_t>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_LL:
      *static_cast<uint64_t *>(dest) = value;
      break;
    default:
      *static_cast<uint32_t *>(dest) = static_cast<uint32_t>(value);
      break;
  }
}

struct Scanset {
  uint8_t bits[32]{};
  bool negate{false};

  bool contains(char c) const {
    auto u = static_cast<uint8_t>(c);
    return ((this->bits[u >> 3] >> (u & 7)) & 1u) != this->negate;
  }
};

// Parses a "[set]" directive starting just after the '[' and returns the
// position after the closing ']'.
const char *parse_scanset(const char *f, Scanset &set) {
  if (*f == '^') {
    set.negate = true;
    f++;
  }
  // A ']' right after '[' or '[^' is a member, not the terminator.
  bool first = true;
  for (; *f != '\0' && (first || *f != ']'); f++) {
    first = false;
    auto lo = static_cast<uint8_t>(*f);
    auto hi = lo;
    if (f[1] == '-' && f[2] != ']' && f[2] != '\0') {
      hi = static_cast<uint8_t>(f[2]);
      f += 2;
    }
    for (unsigned c = lo; c <= hi; c++)
      set.bits[c >> 3] |= static_cast<uint8_t>(1u << (c & 7));
  }
  return *f == ']' ? f + 1 : f;
}

[[noreturn]] void unsupported_conversion() {
  esp_system_abort("sscanf: unsupported conversion; set enable_full_scanf: true in esp32 framework advanced config");
}

// Radix of an integer conversion, 0 meaning "detect from the prefix" (%i).
unsigned int_base(char conv) {
  switch (conv) {
    case 'd':
    case 'u':
      return 10;
    case 'i':
      return 0;
    case 'o':
      return 8;
    case 'x':
    case 'X':
      return 16;
    default:
      unsupported_conversion();
  }
}

int vsscanf_stub(const char *str, const char *fmt, va_list ap) {
  const char *in = str;
  int assigned = 0;
  bool input_failure = false;
  for (const char *f = fmt; *f != '\0'; f++) {
    if (is_space(*f)) {
      while (is_space(*in))
        in++;
      continue;
    }
    bool literal = *f != '%';
    if (!literal && f[1] == '%') {
      // "%%" matches one '%' after skipping input whitespace
      f++;
      while (is_space(*in))
        in++;
      literal = true;
    }
    if (literal) {
      if (*in != *f) {
        input_failure = *in == '\0';
        break;
      }
      in++;
      continue;
    }
    f++;
    bool suppress = false;
    if (*f == '*') {
      suppress = true;
      f++;
    }
    size_t width = 0;
    while (*f >= '0' && *f <= '9') {
      width = width * 10 + static_cast<size_t>(*f - '0');
      f++;
    }
    if (width == 0)
      width = SIZE_MAX;
    auto length = SscanfLength::SSCANF_LENGTH_NONE;
    bool has_length = true;
    switch (*f) {
      case 'h':
        f++;
        if (*f == 'h') {
          length = SscanfLength::SSCANF_LENGTH_HH;
          f++;
        } else {
          length = SscanfLength::SSCANF_LENGTH_H;
        }
        break;
      case 'l':
        f++;
        if (*f == 'l') {
          length = SscanfLength::SSCANF_LENGTH_LL;
          f++;
        }
        break;
      case 'j':
        length = SscanfLength::SSCANF_LENGTH_LL;
        f++;
        break;
      case 'z':
      case 't':
        f++;
        break;
      default:
        has_length = false;
        break;
    }
    const char conv = *f;
    if (conv == '\0')
      break;
    if (conv == 'n') {
      if (!suppress)
        store_int(ap, length, static_cast<unsigned long long>(in - str));
      continue;
    }
    const bool string_conv = conv == 'c' || conv == 's' || conv == '[';
    if (string_conv && has_length)
      unsupported_conversion();  // wide characters
    if (conv != 'c' && conv != '[') {
      while (is_space(*in))
        in++;
    }
    if (*in == '\0') {
      input_failure = true;
      break;
    }
    if (!string_conv) {
      unsigned base = int_base(conv);
      const char *p = in;
      size_t n = 0;
      bool negative = false;
      if (*p == '+' || *p == '-') {
        negative = *p == '-';
        p++;
        n++;
      }
      if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && n + 2 < width &&
          parse_hex_char(p[2]) != INVALID_HEX_CHAR) {
        p += 2;
        n += 2;
        base = 16;
      }
      if (base == 0)
        base = *p == '0' ? 8 : 10;
      unsigned long long value = 0;
      bool any_digit = false;
      for (uint8_t digit; n < width && (digit = parse_hex_char(*p)) < base; p++, n++) {
        value = value * base + digit;
        any_digit = true;
      }
      if (!any_digit)
        break;
      in = p;
      if (!suppress) {
        store_int(ap, length, negative ? 0 - value : value);
        assigned++;
      }
      continue;
    }
    char *dest = suppress ? nullptr : va_arg(ap, char *);
    Scanset set;
    if (conv == '[')
      f = parse_scanset(f + 1, set) - 1;  // the loop's f++ steps past the ']'
    // %c reads exactly width characters (default 1); like newlib, a short
    // read still assigns whatever was available.
    const size_t limit = conv == 'c' && width == SIZE_MAX ? 1 : width;
    size_t n = 0;
    while (n < limit && in[n] != '\0' && (conv == 'c' || (conv == '[' ? set.contains(in[n]) : !is_space(in[n])))) {
      if (dest != nullptr)
        dest[n] = in[n];
      n++;
    }
    if (conv != 'c') {
      if (n == 0)
        break;
      if (dest != nullptr)
        dest[n] = '\0';
    }
    in += n;
    if (!suppress)
      assigned++;
  }
  if (input_failure && assigned == 0)
    return EOF;
  return assigned;
}

}  // namespace

}  // namespace esphome::esp32

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

int __wrap_sscanf(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int result = esphome::esp32::vsscanf_stub(str, fmt, ap);
  va_end(ap);
  return result;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && USE_ESP32_SSCANF_STUB
