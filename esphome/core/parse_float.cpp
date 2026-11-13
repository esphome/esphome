// strtof without libc's strtod (_strtod_l, __gethex and their helpers: about 6.5 KB of flash on ESP8266), giving
// newlib's value and end pointer (C locale) for every input.
//
// newlib's strtof is (float) strtod: the decimal rounds to the nearest double, which then rounds to the nearest
// float. A double approximation picks a candidate float, and exact integer comparisons with the values that strtod
// would round onto a float midpoint settle the result.

#include "esphome/core/helpers.h"

#include <strings.h>
#include <cstring>

namespace esphome {

namespace {

constexpr uint32_t FLOAT_SIGN_BIT = 0x80000000;
constexpr uint32_t FLOAT_INF_BITS = 0x7F800000;
constexpr uint32_t FLOAT_NAN_BITS = 0x7FC00000;  // nanf(""), which newlib's strtof returns for every NaN

// The bounds compared with have at most about 160 significant decimal digits, so later digits only count as
// "something non-zero follows".
constexpr uint32_t EXACT_DIGITS = 200;
// The largest operand: 200 digits (665 bits), or a 55-bit bound times 5^245 (624 bits), shifted to match;
// 21 words is the most the host tests reach.
constexpr uint8_t EXACT_WORDS = 26;

float float_from_bits(uint32_t bits, bool negative) { return bit_cast<float>(negative ? bits | FLOAT_SIGN_BIT : bits); }

bool is_digit(char c) { return static_cast<uint8_t>(c - '0') <= 9; }

bool is_hex_digit(char c) { return parse_hex_char(c) != INVALID_HEX_CHAR; }

uint32_t bit_length(uint64_t value) {
  uint32_t n = 0;
  for (; value != 0; value >>= 1)
    n++;
  return n;
}

// Unsigned integer of up to EXACT_WORDS 32-bit words
class ExactNumber {
 public:
  explicit ExactNumber(uint64_t value) {
    for (; value != 0; value >>= 32)
      this->words_[this->len_++] = static_cast<uint32_t>(value);
  }

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
    uint32_t factor = 1;
    for (; exponent != 0; exponent--)
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
  uint32_t words_[EXACT_WORDS]{};
  uint8_t len_{0};
};

// A non-zero decimal digits * 10^exp10, plus sticky when non-zero digits past EXACT_DIGITS were dropped
struct ExactDecimal {
  ExactNumber digits{0};
  int32_t exp10;
  bool sticky;
};

// The sign of (decimal - mantissa * 2^exp2)
int compare_decimal(const ExactDecimal &d, uint64_t mantissa, int32_t exp2) {
  ExactNumber lhs = d.digits;
  ExactNumber rhs(mantissa);
  int32_t lhs_exp2 = 0;
  if (d.exp10 >= 0) {
    lhs.multiply_pow5(d.exp10);
    lhs_exp2 = d.exp10;
  } else {
    rhs.multiply_pow5(-d.exp10);
    exp2 -= d.exp10;
  }
  if (lhs_exp2 > exp2) {
    lhs.shift_left(lhs_exp2 - exp2);
  } else {
    rhs.shift_left(exp2 - lhs_exp2);
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
  const uint64_t odd = 2 * static_cast<uint64_t>((k & 0x7FFFFF) | (exp != 0 ? 0x800000 : 0)) + 1;
  // As a double, m * 2^m_exp2 with 2^52 <= m < 2^53
  const uint32_t shift = 53 - bit_length(odd);
  const uint64_t m = odd << shift;
  const int32_t m_exp2 = (exp != 0 ? static_cast<int32_t>(exp) - 151 : -150) - static_cast<int32_t>(shift);
  // Below a power of two the next double is half as far away
  if (odd == 1)
    return {4 * m - 1, m_exp2 - 2, 2 * m + 1, m_exp2 - 1};
  return {2 * m - 1, m_exp2 - 1, 2 * m + 1, m_exp2 - 1};
}

uint32_t even_of(uint32_t k) { return (k & 1) != 0 ? k + 1 : k; }

// first..end holds the digits and maybe a point; the value is 0.<digits> * 10^exp10
uint32_t decimal_to_float_bits(const char *first, const char *end, int32_t exp10) {
  // 10^-46 is below half the smallest float, 10^39 above the largest
  if (exp10 <= -46)
    return 0;
  if (exp10 >= 40)
    return FLOAT_INF_BITS;
  ExactDecimal d;
  uint64_t top = 0;  // the first 19 digits, for the first guess
  uint32_t used = 0;
  const char *p = first;
  for (; p < end && used < EXACT_DIGITS; p++) {
    if (*p == '.')
      continue;
    d.digits.multiply_add(10, *p - '0');
    if (used < 19)
      top = top * 10 + (*p - '0');
    used++;
  }
  d.sticky = false;
  for (; p < end && !d.sticky; p++)
    d.sticky = *p >= '1' && *p <= '9';
  d.exp10 = exp10 - static_cast<int32_t>(used);
  // The guess only has to land within a float or two of the result
  const int32_t scale = exp10 - static_cast<int32_t>(used < 19 ? used : 19);
  double power = 1.0;
  for (int32_t i = scale < 0 ? -scale : scale; i > 0; i--)
    power *= 10.0;
  const double guess = scale < 0 ? static_cast<double>(top) / power : static_cast<double>(top) * power;
  uint32_t k = bit_cast<uint32_t>(static_cast<float>(guess));
  for (;;) {
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
}

// newlib's gethex for "0x...". It rounds to 53 bits upwards when the first dropped bit is set and either the kept
// part is odd or a bit below the second dropped bit is set (the second one is not looked at); the double then
// rounds to a float.
float parse_hex_float(const char *str, bool negative, const char **end) {
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
  if (*p == 'p' || *p == 'P') {
    const char *q = p + 1;
    const bool exp_negative = *q == '-';
    if (exp_negative || *q == '+')
      q++;
    if (is_digit(*q)) {
      uint32_t value = 0;
      for (; is_digit(*q); q++)
        value = value * 10 + (*q - '0');
      exp2 += exp_negative ? 0u - value : value;
      p = q;
    }
  }
  *end = p;
  if (zero) {
    if (any_digit)
      return float_from_bits(0, negative);
    // No digit at all: newlib keeps only the leading "0" and drops the sign
    *end = str + 1;
    return 0.0f;
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
    return float_from_bits(FLOAT_INF_BITS, negative);
  if (e < -1074)  // a double below 2^-1021: the float is zero
    return float_from_bits(0, negative);
  if (round_up && ++m == uint64_t{1} << 53) {
    m >>= 1;
    if (++e > 971)
      return float_from_bits(FLOAT_INF_BITS, negative);
  }
  // m * 2^e, a normal double
  const double value = bit_cast<double>((static_cast<uint64_t>(e + 1075) << 52) | (m & ((uint64_t{1} << 52) - 1)));
  const float out = static_cast<float>(value);
  return negative ? -out : out;
}

float parse_float_from(const char *str, const char **end) {
  const char *s = str;
  bool negative = false;
  // Leading white space, then one sign that must be followed by something
  for (;; s++) {
    const char c = *s;
    if (c == ' ' || (c >= '\t' && c <= '\r'))
      continue;
    if (c == '-' || c == '+') {
      negative = c == '-';
      if (*++s != '\0')
        break;
    } else if (c != '\0') {
      break;
    }
    *end = str;
    return 0.0f;
  }
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    return parse_hex_float(s, negative, end);

  const char *p = s;
  bool any_digit = false;
  for (; *p == '0'; p++)
    any_digit = true;
  const char *first = nullptr;  // first non-zero digit
  int32_t exp10 = 0;
  for (; is_digit(*p); p++, exp10++) {
    first = first != nullptr ? first : p;
    any_digit = true;
  }
  const bool point = *p == '.';
  if (point) {
    for (p++; is_digit(*p); p++) {
      any_digit = true;
      if (first == nullptr && *p == '0') {
        exp10--;
        continue;
      }
      first = first != nullptr ? first : p;
    }
  }
  const char *mantissa_end = p;
  if (!any_digit) {
    if (!point && strncasecmp(p, "inf", 3) == 0) {
      p += 3;
      if (strncasecmp(p, "inity", 5) == 0)
        p += 5;
      *end = p;
      return float_from_bits(FLOAT_INF_BITS, negative);
    }
    if (!point && strncasecmp(p, "nan", 3) == 0) {
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
      return float_from_bits(FLOAT_NAN_BITS, negative);
    }
    *end = str;
    return 0.0f;
  }
  if (*p == 'e' || *p == 'E') {
    const char *q = p + 1;
    const bool exp_negative = *q == '-';
    if (exp_negative || *q == '+')
      q++;
    if (is_digit(*q)) {
      // newlib caps the exponent at 19999
      int32_t exp = 0;
      for (; is_digit(*q); q++) {
        if (exp < 20000)
          exp = exp * 10 + (*q - '0');
      }
      if (exp > 19999)
        exp = 19999;
      exp10 += exp_negative ? -exp : exp;
      p = q;
    }
  }
  *end = p;
  if (first == nullptr)
    return float_from_bits(0, negative);
  return float_from_bits(decimal_to_float_bits(first, mantissa_end, exp10), negative);
}

}  // namespace

float parse_float_prefix(const char *str, char **end) {
  const char *stop;
  const float value = parse_float_from(str, &stop);
  if (end != nullptr)
    *end = const_cast<char *>(stop);  // strtof's signature
  return value;
}

}  // namespace esphome
