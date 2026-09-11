/*
 * Linker wrap stub for sscanf() (ESP-IDF, newlib only).
 *
 * Nothing in ESPHome calls sscanf(), but bluedroid does in two places:
 * btc_config_get_bin() decodes stored bonding keys with "%02x" and
 * string_to_bdaddr() parses "%02x:%02x:%02x:%02x:%02x:%02x". Those two
 * calls are the only reference to newlib's scanf engine (__ssvfscanf_r,
 * _strtod_l and their helpers, ~13 KB) in a Bluetooth build on variants
 * whose ROM does not export sscanf.
 *
 * This stub replaces sscanf() with a small scanner covering the integer,
 * character, string and scanset conversions, so any other sscanf() caller
 * in the image (user lambdas included) keeps working. Floating-point
 * conversions are the one gap: supporting them would pull _strtod_l
 * straight back, so a format using %f and friends aborts with a message
 * pointing at enable_full_scanf: true.
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

namespace esphome::esp32 {

namespace {

enum class SscanfLength : uint8_t {
  SSCANF_LENGTH_NONE,
  SSCANF_LENGTH_HH,
  SSCANF_LENGTH_H,
  SSCANF_LENGTH_L,
  SSCANF_LENGTH_LL,
  SSCANF_LENGTH_Z,
  SSCANF_LENGTH_J,
  SSCANF_LENGTH_T,
};

bool is_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

int digit_value(char c, unsigned base) {
  unsigned value;
  if (c >= '0' && c <= '9') {
    value = c - '0';
  } else if (c >= 'a' && c <= 'z') {
    value = c - 'a' + 10;
  } else if (c >= 'A' && c <= 'Z') {
    value = c - 'A' + 10;
  } else {
    return -1;
  }
  return value < base ? static_cast<int>(value) : -1;
}

// Stores through the unsigned counterpart of each length; the bit pattern is
// what the signed conversions want as well.
void store_int(va_list &ap, SscanfLength length, unsigned long long value) {
  switch (length) {
    case SscanfLength::SSCANF_LENGTH_HH:
      *va_arg(ap, unsigned char *) = static_cast<unsigned char>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_H:
      *va_arg(ap, unsigned short *) = static_cast<unsigned short>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_L:
      *va_arg(ap, unsigned long *) = static_cast<unsigned long>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_LL:
    case SscanfLength::SSCANF_LENGTH_J:
      *va_arg(ap, unsigned long long *) = value;
      break;
    case SscanfLength::SSCANF_LENGTH_Z:
      *va_arg(ap, size_t *) = static_cast<size_t>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_T:
      *va_arg(ap, ptrdiff_t *) = static_cast<ptrdiff_t>(value);
      break;
    default:
      *va_arg(ap, unsigned int *) = static_cast<unsigned int>(value);
      break;
  }
}

// Parses a "[set]" directive starting just after the '[' into a 256-bit
// membership bitmap and returns the position after the closing ']'.
const char *parse_scanset(const char *f, uint8_t *bitmap, bool &negate) {
  for (size_t i = 0; i < 32; i++)
    bitmap[i] = 0;
  negate = false;
  if (*f == '^') {
    negate = true;
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
      bitmap[c >> 3] |= static_cast<uint8_t>(1u << (c & 7));
  }
  return *f == ']' ? f + 1 : f;
}

bool in_scanset(const uint8_t *bitmap, bool negate, char c) {
  auto u = static_cast<uint8_t>(c);
  return ((bitmap[u >> 3] >> (u & 7)) & 1u) != negate;
}

[[noreturn]] void unsupported_conversion() {
  esp_system_abort("sscanf: unsupported conversion; set enable_full_scanf: true in esp32 framework advanced config");
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
    if (*f != '%') {
      if (*in != *f) {
        input_failure = *in == '\0';
        break;
      }
      in++;
      continue;
    }
    f++;
    if (*f == '%') {
      while (is_space(*in))
        in++;
      if (*in != '%') {
        input_failure = *in == '\0';
        break;
      }
      in++;
      continue;
    }
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
        } else {
          length = SscanfLength::SSCANF_LENGTH_L;
        }
        break;
      case 'j':
        length = SscanfLength::SSCANF_LENGTH_J;
        f++;
        break;
      case 'z':
        length = SscanfLength::SSCANF_LENGTH_Z;
        f++;
        break;
      case 't':
        length = SscanfLength::SSCANF_LENGTH_T;
        f++;
        break;
      case 'L':
        f++;
        break;
      default:
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
    const bool wide = length == SscanfLength::SSCANF_LENGTH_L || length == SscanfLength::SSCANF_LENGTH_LL;
    if (conv != 'c' && conv != '[') {
      while (is_space(*in))
        in++;
    }
    if (*in == '\0') {
      input_failure = true;
      break;
    }
    unsigned base;
    switch (conv) {
      case 'd':
      case 'u':
        base = 10;
        break;
      case 'i':
        base = 0;
        break;
      case 'o':
        base = 8;
        break;
      case 'x':
      case 'X':
        base = 16;
        break;
      case 'c':
      case 's':
      case '[':
        if (wide)
          unsupported_conversion();
        base = 1;
        break;
      default:
        unsupported_conversion();
    }
    if (base != 1) {
      const char *p = in;
      size_t n = 0;
      bool negative = false;
      if ((*p == '+' || *p == '-') && n < width) {
        negative = *p == '-';
        p++;
        n++;
      }
      if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && n + 2 < width &&
          digit_value(p[2], 16) >= 0) {
        p += 2;
        n += 2;
        base = 16;
      }
      if (base == 0)
        base = *p == '0' ? 8 : 10;
      unsigned long long value = 0;
      bool any_digit = false;
      while (n < width) {
        int digit = digit_value(*p, base);
        if (digit < 0)
          break;
        value = value * base + static_cast<unsigned>(digit);
        p++;
        n++;
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
    if (conv == 'c') {
      // Like newlib, a short read still assigns whatever was available.
      const size_t count = width == SIZE_MAX ? 1 : width;
      size_t n = 0;
      while (n < count && in[n] != '\0') {
        if (dest != nullptr)
          dest[n] = in[n];
        n++;
      }
      in += n;
    } else {
      uint8_t bitmap[32];
      bool negate = false;
      if (conv == '[')
        f = parse_scanset(f + 1, bitmap, negate) - 1;
      size_t n = 0;
      while (n < width && in[n] != '\0' && (conv == '[' ? in_scanset(bitmap, negate, in[n]) : !is_space(in[n]))) {
        if (dest != nullptr)
          dest[n] = in[n];
        n++;
      }
      if (n == 0)
        break;
      if (dest != nullptr)
        dest[n] = '\0';
      in += n;
    }
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
