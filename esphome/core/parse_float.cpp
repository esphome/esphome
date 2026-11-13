// strtof without libc's strtod (_strtod_l, __gethex and their helpers), giving newlib's value and end pointer
// (C locale) for every input. newlib's strtof is (float) strtod: a decimal rounds to the nearest double, which
// then rounds to the nearest float. Exact cases use one float or double operation; otherwise a double guess
// decides unless it lies near a float midpoint, where exact integer comparisons settle it.

#include "esphome/core/parse_float.h"
#include "esphome/core/helpers.h"

#include <strings.h>
#include <algorithm>
#include <cstring>

namespace esphome {

namespace {

constexpr uint32_t FLOAT_SIGN_BIT = 0x80000000;
constexpr uint32_t FLOAT_INF_BITS = 0x7F800000;
constexpr uint32_t FLOAT_NAN_BITS = 0x7FC00000;  // nanf(""), which newlib's strtof returns for every NaN

// The bounds compared with have at most about 160 significant decimal digits, so later digits only count as
// "something non-zero follows".
constexpr uint32_t EXACT_DIGITS = 200;
// The largest operand: 200 digits (665 bits), or a 55-bit bound times 5^245 (624 bits), shifted to match.
constexpr uint8_t EXACT_WORDS = 26;
static_assert(EXACT_WORDS * 32 >= EXACT_DIGITS * 3322 / 1000 + 64, "EXACT_WORDS too small for EXACT_DIGITS");
// The guess is within one float of the result, so the exact comparison moves at most once
constexpr uint32_t MAX_EXACT_STEPS = 3;

#ifdef USE_HOST
uint32_t exact_steps = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): test counter
#endif

bool is_digit(char c) { return static_cast<uint8_t>(c - '0') <= 9; }

bool is_hex_digit(char c) { return parse_hex_char(c) != INVALID_HEX_CHAR; }

uint32_t bit_length(uint64_t value) { return value == 0 ? 0 : 64 - __builtin_clzll(value); }

// 10^n by squaring; exact for float up to 10^10 and for double up to 10^22
template<typename T> T pow10(uint32_t n) {
  T result = 1;
  T base = 10;
  for (;;) {
    if ((n & 1) != 0)
      result *= base;
    n >>= 1;
    if (n == 0)
      return result;
    base *= base;
  }
}

// Unsigned integer of up to EXACT_WORDS 32-bit words
class ExactNumber {
 public:
  explicit ExactNumber(uint64_t value) {
    for (; value != 0; value >>= 32)
      this->words_[this->len_++] = static_cast<uint32_t>(value);
  }
  ExactNumber(const ExactNumber &other) : len_(other.len_) {
    memcpy(this->words_, other.words_, this->len_ * sizeof(uint32_t));
  }
  ExactNumber &operator=(const ExactNumber &) = delete;

  void multiply_add(uint32_t factor, uint32_t addend) {
    uint64_t carry = addend;
    for (uint8_t i = 0; i < this->len_; i++) {
      carry += static_cast<uint64_t>(this->words_[i]) * factor;
      this->words_[i] = static_cast<uint32_t>(carry);
      carry >>= 32;
    }
    if (carry != 0)
      this->words_[this->len_++] = static_cast<uint32_t>(carry);
  }

  void multiply_pow5(uint32_t exponent) {
    static constexpr uint32_t POW5_13 = 1220703125;  // the largest power of 5 in 32 bits
    for (; exponent >= 13; exponent -= 13)
      this->multiply_add(POW5_13, 0);
    if (exponent == 0)
      return;
    uint32_t factor = 5;
    for (; exponent > 1; exponent--)
      factor *= 5;
    this->multiply_add(factor, 0);
  }

  void shift_left(uint32_t bits) {
    const uint32_t words = bits / 32;
    const uint32_t rem = bits % 32;
    if (rem != 0) {
      uint32_t carry = 0;
      for (uint8_t i = 0; i < this->len_; i++) {
        const uint32_t word = this->words_[i];
        this->words_[i] = (word << rem) | carry;
        carry = word >> (32 - rem);
      }
      if (carry != 0)
        this->words_[this->len_++] = carry;
    }
    memmove(this->words_ + words, this->words_, this->len_ * sizeof(uint32_t));
    memset(this->words_, 0, words * sizeof(uint32_t));
    this->len_ += words;
  }

  int compare(const ExactNumber &other) const {
    if (this->len_ != other.len_)
      return this->len_ < other.len_ ? -1 : 1;
    for (uint8_t i = this->len_; i-- > 0;) {
      if (this->words_[i] != other.words_[i])
        return this->words_[i] < other.words_[i] ? -1 : 1;
    }
    return 0;
  }

 private:
  uint32_t words_[EXACT_WORDS];
  uint8_t len_{0};
};

// The parsed decimal 0.<digits> * 10^exp10; first..end holds the digits and maybe a point
struct Decimal {
  const char *first;
  const char *end;
  uint64_t top;     // the first 19 digits
  uint32_t digits;  // all of them
  int32_t exp10;
};

// A non-zero decimal digits * 2^exp2 / 5^pow5, plus sticky when non-zero digits past EXACT_DIGITS were dropped
struct ExactDecimal {
  ExactNumber digits{0};
  int32_t exp2;
  uint32_t pow5;
  bool sticky;
};

// The sign of (decimal - mantissa * 2^exp2)
int compare_decimal(const ExactDecimal &d, uint64_t mantissa, int32_t exp2) {
  ExactNumber lhs(d.digits);
  ExactNumber rhs(mantissa);
  rhs.multiply_pow5(d.pow5);
  if (d.exp2 > exp2) {
    lhs.shift_left(d.exp2 - exp2);
  } else {
    rhs.shift_left(exp2 - d.exp2);
  }
  const int order = lhs.compare(rhs);
  return order == 0 && d.sticky ? 1 : order;
}

// strtod rounds every value in [low, high] onto the midpoint between the floats with bits k and k + 1 (the
// bounds are the midpoints to the neighbouring doubles), and the float conversion takes that to the even float.
struct MidpointRange {
  uint64_t low_mantissa;
  int32_t low_exp2;
  uint64_t high_mantissa;
  int32_t high_exp2;
};

MidpointRange midpoint_range(uint32_t k) {
  const uint32_t exp = k >> 23;
  const uint32_t odd = 2 * ((k & 0x7FFFFF) | (exp != 0 ? 0x800000 : 0)) + 1;
  // As a double, m * 2^m_exp2 with 2^52 <= m < 2^53
  const uint32_t shift = 53 - bit_length(odd);
  const uint64_t m = static_cast<uint64_t>(odd) << shift;
  const int32_t m_exp2 = (exp != 0 ? static_cast<int32_t>(exp) - 151 : -150) - static_cast<int32_t>(shift);
  // Below a power of two the next double is half as far away
  if (odd == 1)
    return {4 * m - 1, m_exp2 - 2, 2 * m + 1, m_exp2 - 1};
  return {2 * m - 1, m_exp2 - 1, 2 * m + 1, m_exp2 - 1};
}

uint32_t even_of(uint32_t k) { return k + (k & 1); }

// Out of line and cold: ordinary input never runs it, so it stays out of the flash cache
__attribute__((noinline, cold)) uint32_t exact_float_bits(const Decimal &in, uint32_t k) {
  ExactDecimal d;
  uint32_t used = 0;
  const char *p = in.first;
  while (p < in.end && used < EXACT_DIGITS) {
    // Up to 9 digits per pass
    uint32_t chunk = 0;
    uint32_t n = 0;
    for (; p < in.end && n < 9 && used < EXACT_DIGITS; p++) {
      if (*p == '.')
        continue;
      chunk = chunk * 10 + (*p - '0');
      n++;
      used++;
    }
    d.digits.multiply_add(pow10<uint32_t>(n), chunk);
  }
  d.sticky = false;
  for (; p < in.end && !d.sticky; p++)
    d.sticky = *p >= '1' && *p <= '9';
  // decimal = digits * 10^e = digits * 2^e * 5^e
  const int32_t e = in.exp10 - static_cast<int32_t>(used);
  d.exp2 = e;
  d.pow5 = e < 0 ? -e : 0;
  if (e >= 0)
    d.digits.multiply_pow5(e);
  for (uint32_t step = 0; step < MAX_EXACT_STEPS; step++) {
#ifdef USE_HOST
    exact_steps++;
#endif
    if (k < FLOAT_INF_BITS) {
      const MidpointRange up = midpoint_range(k);
      if (compare_decimal(d, up.low_mantissa, up.low_exp2) >= 0) {
        if (compare_decimal(d, up.high_mantissa, up.high_exp2) <= 0)
          return even_of(k);
        k++;
        continue;
      }
    }
    if (k > 0) {
      const MidpointRange down = midpoint_range(k - 1);
      if (compare_decimal(d, down.high_mantissa, down.high_exp2) <= 0) {
        if (compare_decimal(d, down.low_mantissa, down.low_exp2) >= 0)
          return even_of(k - 1);
        k--;
        continue;
      }
    }
    return k;
  }
  return k;
}

uint32_t decimal_to_float_bits(const Decimal &in) {
  // 10^-46 is below half the smallest float, 10^39 above the largest
  if (in.exp10 <= -46)
    return 0;
  if (in.exp10 >= 40)
    return FLOAT_INF_BITS;
  const int32_t scale = in.exp10 - static_cast<int32_t>(std::min<uint32_t>(in.digits, 19));
  const uint32_t power = scale < 0 ? -scale : scale;
  // Both operands are exact floats, and a double has more than 2 * 24 + 1 bits, so one float operation gives
  // the same float as the double operation strtod does followed by the float conversion (Figueroa)
  if (in.digits <= 19 && in.top < (1u << 24) && power <= 10) {
    const auto value = static_cast<float>(static_cast<uint32_t>(in.top));
    const float factor = pow10<float>(power);
    return bit_cast<uint32_t>(scale < 0 ? value / factor : value * factor);
  }
  const auto value = static_cast<double>(in.top);
  const double factor = pow10<double>(power);
  const double guess = scale < 0 ? value / factor : value * factor;
  const uint32_t k = internal::double_to_float_bits(guess);
  // Both exact doubles (up to 2^53 and 10^22): the one double operation is strtod's correctly rounded double
  if (in.digits <= 19 && in.top <= (uint64_t{1} << 53) && power <= 22)
    return k;
  // Otherwise within 13 doubles of the decimal: the digits' conversion, the dropped digits and the last operation
  // cost one each, and 10^n at most 10 (10^32 and 10^64 are inexact, plus up to 6 multiplications). 64 doubles
  // either side that round to the same float leave strtod no other choice.
  const uint64_t guess_bits = bit_cast<uint64_t>(guess);
  if (internal::double_to_float_bits(bit_cast<double>(guess_bits - 64)) == k &&
      internal::double_to_float_bits(bit_cast<double>(guess_bits + 64)) == k)
    return k;
  return exact_float_bits(in, k);
}

// Reads [eE]-style exponent digits after a sign at p; returns the end, or nullptr when no digit follows
const char *read_exponent(const char *p, bool *negative) {
  *negative = *p == '-';
  if (*negative || *p == '+')
    p++;
  return is_digit(*p) ? p : nullptr;
}

// newlib's gethex for "0x...". It rounds to 53 bits upwards when the first dropped bit is set and either the kept
// part is odd or a bit below the second dropped bit is set (the second one is not looked at); the double then
// rounds to a float. Clears negative when nothing is taken but the "0".
__attribute__((noinline, cold)) uint32_t parse_hex_float(const char *str, bool *negative, const char **end) {
  const char *p = str + 2;
  bool any_digit = false;
  for (; *p == '0'; p++)
    any_digit = true;
  const char *first = p;
  const char *point = nullptr;
  bool zero = false;
  bool scan = true;
  uint32_t exp2 = 0;  // newlib's exponent is a 32-bit Long; uint32_t wraps the same way
  if (!is_hex_digit(*p)) {
    // No digit before a point; one after it carries on, and a bare point is still consumed
    zero = true;
    scan = false;
    if (*p == '.') {
      point = ++p;
      if (is_hex_digit(*p)) {
        for (; *p == '0'; p++) {
        }
        zero = !is_hex_digit(*p);
        any_digit = true;
        first = p;
        scan = true;
      }
    }
  }
  if (scan) {
    for (; is_hex_digit(*p); p++) {
    }
    if (*p == '.' && point == nullptr) {
      point = ++p;
      for (; is_hex_digit(*p); p++) {
      }
    }
    if (point != nullptr)
      exp2 = 0u - (static_cast<uint32_t>(p - point) << 2);
  }
  const char *mantissa_end = p;
  bool exp_negative = false;
  const char *exp_digits = *p == 'p' || *p == 'P' ? read_exponent(p + 1, &exp_negative) : nullptr;
  if (exp_digits != nullptr) {
    uint32_t value = 0;
    for (; is_digit(*exp_digits); exp_digits++)
      value = value * 10 + (*exp_digits - '0');
    exp2 += exp_negative ? 0u - value : value;
    p = exp_digits;
  }
  *end = p;
  if (zero) {
    if (!any_digit) {
      // No digit at all: newlib keeps only the leading "0" and drops the sign
      *end = str + 1;
      *negative = false;
    }
    return 0;
  }
  // The first 16 digits in full; the bits of later ones all lie below the second dropped bit
  uint64_t acc = 0;
  uint32_t digits = 0;
  bool tail_set = false;
  for (const char *q = first; q < mantissa_end; q++) {
    if (*q == '.')
      continue;
    const uint8_t v = parse_hex_char(*q);
    if (digits < 16) {
      acc = (acc << 4) | v;
    } else {
      tail_set |= v != 0;
    }
    digits++;
  }
  const uint32_t acc_bits = bit_length(acc);
  const uint32_t bits = acc_bits + 4 * (digits > 16 ? digits - 16 : 0);
  uint64_t m;
  bool round_up = false;
  if (bits <= 53) {
    m = acc << (53 - bits);
    exp2 -= 53 - bits;
  } else {
    exp2 += bits - 53;
    const uint32_t shift = acc_bits - 53;  // at least 1
    m = acc >> shift;
    const bool first_dropped = ((acc >> (shift - 1)) & 1) != 0;
    const bool below_second = tail_set || (shift >= 3 && (acc & ((uint64_t{1} << (shift - 2)) - 1)) != 0);
    round_up = first_dropped && (below_second || (m & 1) != 0);
  }
  int32_t e = static_cast<int32_t>(exp2);
  if (e > 971)
    return FLOAT_INF_BITS;
  if (e < -1074)  // a double below 2^-1021: the float is zero
    return 0;
  if (round_up && ++m == uint64_t{1} << 53) {
    m >>= 1;
    if (++e > 971)
      return FLOAT_INF_BITS;
  }
  // m * 2^e, a normal double
  return internal::double_to_float_bits(
      bit_cast<double>((static_cast<uint64_t>(e + 1075) << 52) | (m & ((uint64_t{1} << 52) - 1))));
}

// An optional sign, up to 24 bits of digits and 9 after the point, in one float operation; nullptr otherwise.
ESPHOME_ALWAYS_INLINE inline const char *parse_plain_decimal(const char *str, uint32_t *bits, bool *negative) {
  const char *p = str;
  *negative = *p == '-';
  if (*negative || *p == '+')
    p++;
  const char *digits = p;
  const char *point = nullptr;
  uint32_t mantissa = 0;
  for (;; p++) {
    const uint32_t d = static_cast<uint8_t>(*p) - '0';
    if (d > 9) {
      if (*p == '.' && point == nullptr) {
        point = p;
        continue;
      }
      break;
    }
    if (mantissa > 1677720)  // the next digit could pass 2^24
      return nullptr;
    mantissa = mantissa * 10 + d;
  }
  const uint32_t fraction = point != nullptr ? p - point - 1 : 0;
  const char c = static_cast<char>(*p | 0x20);
  if (p - digits == (point != nullptr ? 1 : 0) || fraction > 9 || c == 'e' || c == 'x')
    return nullptr;
  float value = static_cast<float>(mantissa);
  if (fraction != 0)
    value /= pow10<float>(fraction);  // exact up to 10^10
  *bits = bit_cast<uint32_t>(value);
  return p;
}

uint32_t no_number(const char *str, const char **end, bool *negative) {
  *end = str;
  *negative = false;
  return 0;
}

// Everything but a plain decimal; returns the bits without the sign, which goes in *negative
__attribute__((noinline)) uint32_t parse_float_full(const char *str, const char **end, bool *negative) {
  // No number starts with a letter but i (inf) or n (nan): "unavailable", "on" and "unknown" stop here
  const char lower = static_cast<char>(*str | 0x20);
  if (lower >= 'a' && lower <= 'z' && lower != 'i' && lower != 'n')
    return no_number(str, end, negative);
  const char *s = str;
  *negative = false;
  // Leading white space, then one sign that must be followed by something
  for (;; s++) {
    const char c = *s;
    if (c == ' ' || (c >= '\t' && c <= '\r'))
      continue;
    if (c == '-' || c == '+') {
      *negative = c == '-';
      if (*++s != '\0')
        break;
    } else if (c != '\0') {
      break;
    }
    return no_number(str, end, negative);
  }
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    return parse_hex_float(s, negative, end);

  const char *p = s;
  bool any_digit = false;
  for (; *p == '0'; p++)
    any_digit = true;
  Decimal d{nullptr, nullptr, 0, 0, 0};
  // Separate loops before and after the point: one merged loop costs a few instructions per digit
  for (; is_digit(*p); p++, d.exp10++) {
    d.first = d.first != nullptr ? d.first : p;
    if (d.digits < 19)
      d.top = d.top * 10 + (*p - '0');
    d.digits++;
    any_digit = true;
  }
  const bool point = *p == '.';
  if (point) {
    for (p++; is_digit(*p); p++) {
      any_digit = true;
      // Zeros before the first other digit only move the point
      if (d.first == nullptr && *p == '0') {
        d.exp10--;
        continue;
      }
      d.first = d.first != nullptr ? d.first : p;
      if (d.digits < 19)
        d.top = d.top * 10 + (*p - '0');
      d.digits++;
    }
  }
  d.end = p;
  if (!any_digit) {
    const char first_letter = static_cast<char>(*p | 0x20);
    if (!point && first_letter == 'i' && strncasecmp(p, "inf", 3) == 0) {
      p += 3;
      if (strncasecmp(p, "inity", 5) == 0)
        p += 5;
      *end = p;
      return FLOAT_INF_BITS;
    }
    if (!point && first_letter == 'n' && strncasecmp(p, "nan", 3) == 0) {
      p += 3;
      // "nan(...)" is taken when the parentheses hold only hex digits and characters up to ' '
      if (*p == '(') {
        for (const char *q = p + 1; *q != '\0' && (is_hex_digit(*q) || static_cast<uint8_t>(*q) <= ' ' || *q == ')');
             q++) {
          if (*q == ')') {
            p = q + 1;
            break;
          }
        }
      }
      *end = p;
      return FLOAT_NAN_BITS;
    }
    return no_number(str, end, negative);
  }
  bool exp_negative = false;
  const char *q = *p == 'e' || *p == 'E' ? read_exponent(p + 1, &exp_negative) : nullptr;
  if (q != nullptr) {
    // newlib caps the exponent at 19999
    int32_t exp = 0;
    for (; is_digit(*q); q++) {
      if (exp < 20000)
        exp = exp * 10 + (*q - '0');
    }
    if (exp > 19999)
      exp = 19999;
    d.exp10 += exp_negative ? -exp : exp;
    p = q;
  }
  *end = p;
  return d.first == nullptr ? 0 : decimal_to_float_bits(d);
}

// Returns the bits without the sign, which goes in *negative
ESPHOME_ALWAYS_INLINE inline uint32_t parse_float_bits(const char *str, const char **end, bool *negative) {
  uint32_t plain;
  if (const char *stop = parse_plain_decimal(str, &plain, negative)) {
    *end = stop;
    return plain;
  }
  return parse_float_full(str, end, negative);
}

}  // namespace

namespace internal {

uint32_t double_to_float_bits(double value) {
  const uint64_t bits = bit_cast<uint64_t>(value);
  const int32_t exp2 = static_cast<int32_t>(bits >> 52) - 1023;  // value = 1.m * 2^exp2
  if (exp2 >= 128)
    return FLOAT_INF_BITS;
  if (exp2 < -150)  // below half the smallest float
    return 0;
  const uint64_t m = (bits & ((uint64_t{1} << 52) - 1)) | (uint64_t{1} << 52);
  // Keep 24 bits for a normal float, fewer below 2^-126 so the unit is 2^-149
  const uint32_t shift = exp2 >= -126 ? 29 : static_cast<uint32_t>(29 - 126 - exp2);
  uint64_t q = m >> shift;
  const uint64_t rem = m & ((uint64_t{1} << shift) - 1);
  const uint64_t half = uint64_t{1} << (shift - 1);
  if (rem > half || (rem == half && (q & 1) != 0))
    q++;
  if (exp2 < -126)
    return static_cast<uint32_t>(q);  // 2^23 here is the smallest normal float, as it should be
  // Rounding up past the largest float gives exactly FLOAT_INF_BITS
  return (static_cast<uint32_t>(exp2 + 127) << 23) + static_cast<uint32_t>(q - (uint64_t{1} << 23));
}

#ifdef USE_HOST
uint32_t parse_float_exact_steps() { return exact_steps; }
#endif

}  // namespace internal

optional<float> parse_float(const char *str) {
  // Its own loop for the whole string as a plain decimal (up to 7 digits and 9 after the point): sharing
  // parse_plain_decimal costs 12 to 18% more instructions on plain decimals, the most common input here
  const char *p = str;
  const bool negative = *p == '-';
  if (negative || *p == '+')
    p++;
  const char *digits = p;
  const char *point = nullptr;
  uint32_t mantissa = 0;
  for (;; p++) {
    const uint32_t d = static_cast<uint8_t>(*p) - '0';
    if (d > 9) {
      if (*p == '.' && point == nullptr) {
        point = p;
        continue;
      }
      break;
    }
    if (mantissa >= 1000000)  // one more digit stays below 2^24
      break;
    mantissa = mantissa * 10 + d;
  }
  const uint32_t fraction = point != nullptr ? p - point - 1 : 0;
  if (*p == '\0' && p - digits > (point != nullptr ? 1 : 0) && fraction <= 9) {
    float value = static_cast<float>(mantissa);
    if (negative)
      value = -value;
    if (fraction != 0)
      value /= pow10<float>(fraction);
    return value;
  }
  const char *end;
  bool full_negative;
  const uint32_t bits = parse_float_full(str, &end, &full_negative);
  // HUGE_VALF, positive overflow or "inf", is rejected like before
  if (end == str || *end != '\0' || (!full_negative && bits == FLOAT_INF_BITS))
    return {};
  return bit_cast<float>(full_negative ? bits | FLOAT_SIGN_BIT : bits);
}

float parse_float_prefix(const char *str, char **end) {
  const char *stop;
  bool negative;
  const uint32_t bits = parse_float_bits(str, &stop, &negative);
  if (end != nullptr)
    *end = const_cast<char *>(stop);  // strtof's signature
  return bit_cast<float>(negative ? bits | FLOAT_SIGN_BIT : bits);
}

}  // namespace esphome
