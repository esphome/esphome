#pragma once

/*
 * A small sscanf without floating-point conversions.
 *
 * Backs the ESP-IDF linker wrap in components/esp32/sscanf_stubs.cpp, where
 * bluedroid's "%02x" parses would otherwise link newlib's whole scanf engine
 * (~13 KB with _strtod_l). Integer, character, string and scanset conversions
 * behave like libc; a conversion this scanner does not implement (floats,
 * %p, wide characters) returns SSCANF_UNSUPPORTED so the caller can fail
 * loudly instead of misparsing.
 *
 * Header only so the host unit tests can exercise it without compiling a
 * target platform's sources.
 */

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "esphome/core/helpers.h"

namespace esphome {

/// Returned by vsscanf_no_float() for a conversion it does not implement.
static constexpr int SSCANF_UNSUPPORTED = -2;

namespace sscanf_no_float_internal {

enum class SscanfLength : uint8_t {
  SSCANF_LENGTH_NONE,
  SSCANF_LENGTH_HH,
  SSCANF_LENGTH_H,
  SSCANF_LENGTH_L,
  SSCANF_LENGTH_LL,
  SSCANF_LENGTH_Z,
  SSCANF_LENGTH_T,
};

inline bool is_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

// The bit pattern is what the signed conversions want as well. All object
// pointers share one representation, so the caller fetches the argument
// once as void * rather than once per pointee type.
inline void store_int(void *dest, SscanfLength length, unsigned long long value) {
  switch (length) {
    case SscanfLength::SSCANF_LENGTH_HH:
      *static_cast<unsigned char *>(dest) = static_cast<unsigned char>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_H:
      *static_cast<unsigned short *>(dest) = static_cast<unsigned short>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_L:
      *static_cast<unsigned long *>(dest) = static_cast<unsigned long>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_LL:
      *static_cast<unsigned long long *>(dest) = value;
      break;
    case SscanfLength::SSCANF_LENGTH_Z:
      *static_cast<size_t *>(dest) = static_cast<size_t>(value);
      break;
    case SscanfLength::SSCANF_LENGTH_T:
      *static_cast<ptrdiff_t *>(dest) = static_cast<ptrdiff_t>(value);
      break;
    default:
      *static_cast<unsigned int *>(dest) = static_cast<unsigned int>(value);
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
inline const char *parse_scanset(const char *f, Scanset &set) {
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

// Radix of an integer conversion: 0 means "detect from the prefix" (%i),
// 0xFF marks a conversion this scanner does not implement.
inline unsigned int_base(char conv) {
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
      return 0xFF;
  }
}

}  // namespace sscanf_no_float_internal

/// sscanf semantics for the integer, character, string and scanset
/// conversions; returns SSCANF_UNSUPPORTED on any other conversion.
inline int vsscanf_no_float(const char *str, const char *fmt, va_list ap) {
  using namespace sscanf_no_float_internal;
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
        } else {
          length = SscanfLength::SSCANF_LENGTH_L;
        }
        break;
      case 'j':
        length = SscanfLength::SSCANF_LENGTH_LL;
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
      default:
        has_length = false;
        break;
    }
    const char conv = *f;
    if (conv == '\0')
      break;
    if (conv == 'n') {
      if (!suppress)
        store_int(va_arg(ap, void *), length, static_cast<unsigned long long>(in - str));
      continue;
    }
    const bool string_conv = conv == 'c' || conv == 's' || conv == '[';
    if (string_conv && has_length)
      return SSCANF_UNSUPPORTED;  // wide characters
    unsigned base = 0;
    if (!string_conv && (base = int_base(conv)) == 0xFF)
      return SSCANF_UNSUPPORTED;
    if (conv != 'c' && conv != '[') {
      while (is_space(*in))
        in++;
    }
    if (*in == '\0') {
      input_failure = true;
      break;
    }
    if (!string_conv) {
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
        store_int(va_arg(ap, void *), length, negative ? 0 - value : value);
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

/// Variadic form of vsscanf_no_float().
inline int sscanf_no_float(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int result = vsscanf_no_float(str, fmt, ap);
  va_end(ap);
  return result;
}

}  // namespace esphome
