// strtof without libc's strtod (_strtod_l, __gethex and their helpers): the correctly rounded float (nearest, ties
// to even) with newlib's syntax and end pointer, the same on every platform. Plain
// decimals take one float operation; otherwise a double guess decides unless the decimal lies near a float
// midpoint, where an exact integer comparison settles it. Float bits are built with integer operations only.

#include "esphome/core/parse_float.h"
#include "esphome/core/helpers.h"

#include <strings.h>
#include <algorithm>
#include <bit>
#include <cstring>

namespace esphome {

namespace {

constexpr uint32_t FLOAT_SIGN_BIT = 0x80000000;
constexpr uint32_t FLOAT_INF_BITS = 0x7F800000;
constexpr uint32_t FLOAT_NAN_BITS = 0x7FC00000;  // nanf(""), which strtof returns for every NaN

// 0.<digits> * 10^exp10 is below 10^-46, under half the smallest float, when exp10 <= MIN_EXP10, and at least
// 10^39, over the largest float, when exp10 >= MAX_EXP10
constexpr int32_t MIN_EXP10 = -46;
constexpr int32_t MAX_EXP10 = 40;
// A float midpoint has at most about 150 significant decimal digits, so later digits only count as "something
// non-zero follows".
constexpr uint32_t EXACT_DIGITS = 200;
constexpr uint8_t EXACT_WORDS = 26;
// Both operands: the digits (log2(10) bits each), or a 25-bit midpoint times 5^(EXACT_DIGITS - MIN_EXP10)
// (log2(5) bits each), plus up to 64 bits of shift to line them up
static_assert(EXACT_WORDS * 32 >=
                  std::max(EXACT_DIGITS * 3322 / 1000, 25 + (EXACT_DIGITS - MIN_EXP10) * 2322 / 1000) + 64,
              "EXACT_WORDS too small for EXACT_DIGITS and MIN_EXP10");
// The guess is within one float of the result, so the exact comparison moves at most once: two rounds, and a
// third to spare
constexpr uint32_t MAX_EXACT_STEPS = 3;

#ifdef USE_HOST
uint32_t exact_steps = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): test counter
#endif

bool is_hex_digit(char c) { return parse_hex_char(c) != INVALID_HEX_CHAR; }

// The float nearest to (m, plus something below 1 when sticky) * 2^e, ties to even, as bits; m > 0, and sticky only
// when bits are dropped. Integer operations only: some chips flush float denormals in their own conversion.
uint32_t round_to_float_bits(uint64_t m, int32_t e, bool sticky) {
  const int32_t msb = e + static_cast<int32_t>(std::bit_width(m)) - 1;  // the value is in [2^msb, 2^(msb + 1))
  if (msb >= 128)
    return FLOAT_INF_BITS;
  if (msb < -150)  // below half the smallest float
    return 0;
  // The unit of the float's last bit: 2^(msb - 23), or 2^-149 for a denormal
  const int32_t shift = std::max<int32_t>(msb - 23, -149) - e;  // at most 64
  uint64_t q;
  if (shift <= 0) {
    q = m << -shift;
  } else {
    q = shift < 64 ? m >> shift : 0;
    const uint64_t rem = shift < 64 ? m & ((uint64_t{1} << shift) - 1) : m;
    const uint64_t half = uint64_t{1} << (shift - 1);
    if (rem > half || (rem == half && (sticky || (q & 1) != 0)))
      q++;
  }
  if (msb < -126)
    return static_cast<uint32_t>(q);  // 2^23 here is the smallest normal float, as it should be
  // Rounding up past the largest float gives exactly FLOAT_INF_BITS
  return (static_cast<uint32_t>(msb + 127) << 23) + static_cast<uint32_t>(q - (uint64_t{1} << 23));
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

// The midpoint between the floats with bits k and k + 1, odd * 2^exp2
struct Midpoint {
  uint32_t odd;
  int32_t exp2;
};

Midpoint midpoint(uint32_t k) {
  const uint32_t exp = k >> 23;
  return {2 * ((k & 0x7FFFFF) | (exp != 0 ? 0x800000 : 0)) + 1, exp != 0 ? static_cast<int32_t>(exp) - 151 : -150};
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
      const Midpoint up = midpoint(k);
      const int order = compare_decimal(d, up.odd, up.exp2);
      if (order > 0) {
        k++;
        continue;
      }
      if (order == 0)
        return even_of(k);
    }
    if (k > 0) {
      const Midpoint down = midpoint(k - 1);
      const int order = compare_decimal(d, down.odd, down.exp2);
      if (order < 0) {
        k--;
        continue;
      }
      if (order == 0)
        return even_of(k - 1);
    }
    return k;
  }
  return k;
}

uint32_t decimal_to_float_bits(const Decimal &in) {
  if (in.exp10 <= MIN_EXP10)
    return 0;
  if (in.exp10 >= MAX_EXP10)
    return FLOAT_INF_BITS;
  const int32_t scale = in.exp10 - static_cast<int32_t>(std::min<uint32_t>(in.digits, 19));
  const uint32_t power = scale < 0 ? -scale : scale;
  // Both operands are exact floats, so the one float operation rounds the decimal correctly
  if (in.digits <= 19 && in.top < (1u << 24) && power <= 10) {
    const auto value = static_cast<float>(static_cast<uint32_t>(in.top));
    const auto factor = static_cast<float>(pow10<uint64_t>(power));  // exact: 10^10 is a float
    return bit_cast<uint32_t>(scale < 0 ? value / factor : value * factor);
  }
  // The guess is within 13 doubles of the decimal: the digits' conversion and the dropped digits cost one each,
  // 10^n at most 10 (10^32 and 10^64 are inexact, plus up to 6 multiplications) and the last operation one. More
  // than 64 doubles from a float midpoint, the decimal rounds to the same float as the guess.
  const auto value = static_cast<double>(in.top);
  const double factor = pow10<double>(power);
  const uint64_t guess = bit_cast<uint64_t>(scale < 0 ? value / factor : value * factor);
  const int32_t exp2 = static_cast<int32_t>(guess >> 52) - 1075;  // guess = m * 2^exp2, over 2^-155, so shift <= 58
  const uint64_t m = (guess & ((uint64_t{1} << 52) - 1)) | (uint64_t{1} << 52);
  // The bits below the float's last one, and the midpoint among them
  const int32_t shift = std::max<int32_t>(29, -149 - exp2);
  const uint64_t low = m & ((uint64_t{1} << shift) - 1);
  const uint64_t half = uint64_t{1} << (shift - 1);
  const uint32_t k = round_to_float_bits(m, exp2, false);
  if (low - half + 64 > 128)  // |low - half| > 64
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

// "0x...", with newlib's syntax and end pointer, correctly rounded. Clears negative when nothing is taken but the
// "0".
__attribute__((noinline, cold)) uint32_t parse_hex_float(const char *str, bool *negative, const char **end) {
  const char *p = str + 2;
  bool any_digit = false;
  for (; *p == '0'; p++)
    any_digit = true;
  const char *first = p;
  const char *point = nullptr;
  bool zero = false;
  bool scan = true;
  int32_t exp2 = 0;
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
      exp2 = -4 * static_cast<int32_t>(p - point);
  }
  const char *mantissa_end = p;
  bool exp_negative = false;
  const char *exp_digits = *p == 'p' || *p == 'P' ? read_exponent(p + 1, &exp_negative) : nullptr;
  if (exp_digits != nullptr) {
    int32_t value = 0;
    for (; is_digit(*exp_digits); exp_digits++) {
      if (value < 100000)  // far past any float either way
        value = value * 10 + (*exp_digits - '0');
    }
    exp2 += exp_negative ? -value : value;
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
  // The first 16 digits in full; later ones only count as "something non-zero follows"
  uint64_t acc = 0;
  uint32_t digits = 0;
  bool sticky = false;
  for (const char *q = first; q < mantissa_end; q++) {
    if (*q == '.')
      continue;
    const uint8_t v = parse_hex_char(*q);
    if (digits < 16) {
      acc = (acc << 4) | v;
      digits++;
    } else {
      sticky |= v != 0;
      exp2 += 4;
    }
  }
  return round_to_float_bits(acc, exp2, sticky);
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
    value /= static_cast<float>(pow10<uint32_t>(fraction));  // 10^9 is an exact float
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
    if (is_space_c(c))
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
  const auto exp = static_cast<int32_t>((bits >> 52) & 0x7FF);
  if (exp == 0)  // zero, or a denormal double: far below the smallest float
    return 0;
  return round_to_float_bits((bits & ((uint64_t{1} << 52) - 1)) | (uint64_t{1} << 52), exp - 1075, false);
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
      value /= static_cast<float>(pow10<uint32_t>(fraction));
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

float str_to_float(const char *str, char **end) {
  const char *stop;
  bool negative;
  const uint32_t bits = parse_float_bits(str, &stop, &negative);
  if (end != nullptr)
    *end = const_cast<char *>(stop);  // strtof's signature
  return bit_cast<float>(negative ? bits | FLOAT_SIGN_BIT : bits);
}

}  // namespace esphome
