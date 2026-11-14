// strtof without libc's strtod (_strtod_l, __gethex and their helpers): picolibc's strtof (1.8.10, the one in
// ESP-IDF 6), bit for bit and with its end pointer, on every platform. A decimal is rounded to 9 significant
// digits (ties to even) and those are converted to the nearest float; hex is rounded to the nearest float. No
// double arithmetic, and float bits are built with integer operations only.

#include "esphome/core/parse_float.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <bit>

namespace esphome {

namespace {

constexpr uint32_t FLOAT_SIGN_BIT = 0x80000000;
constexpr uint32_t FLOAT_INF_BITS = 0x7F800000;
constexpr uint32_t FLOAT_NAN_BITS = 0x7FC00000;  // nanf(""), which strtof returns for every NaN

// Digits kept before the rest only round: 9 decimal digits, 8 hex digits
constexpr uint32_t DECIMAL_DIGITS = 9;
constexpr uint32_t HEX_DIGITS = 8;
// picolibc stops adding exponent digits once the exponent reaches this
constexpr int32_t EXPONENT_CAP = 224;
// 0.<digits> * 10^exp10 is zero when exp10 <= MIN_EXP10 and infinity when exp10 >= MAX_EXP10, as picolibc decides
constexpr int32_t MIN_EXP10 = -46;
constexpr int32_t MAX_EXP10 = 40;
// A plain-decimal mantissa above this could pass 2^24 with one more digit, so the float would no longer be exact
constexpr uint32_t PLAIN_MANTISSA_LIMIT = 1677720;

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

// The decimal exponents that reach decimal_to_float_bits: 1 to 9 digits and MIN_EXP10 < digits + e10 < MAX_EXP10
constexpr int32_t MIN_E10 = MIN_EXP10 + 1 - static_cast<int32_t>(DECIMAL_DIGITS);
constexpr int32_t MAX_E10 = MAX_EXP10 - 2;
// 10^(8k) for k = POW10_BY_8_FIRST..POW10_BY_8_LAST as the top 64 bits of its binary expansion (rounded down):
// 10^(8k) = POW10_BY_8[k - POW10_BY_8_FIRST] * 2^(floor_log2_pow10(8k) - 63), exact for k = 0..3
constexpr int32_t POW10_BY_8_FIRST = MIN_E10 >> 3;
constexpr int32_t POW10_BY_8_LAST = MAX_E10 >> 3;
constexpr uint64_t POW10_BY_8[] PROGMEM = {
    0xFB158592BE068D2E, 0xBB127C53B17EC159, 0x8B61313BBABCE2C6, 0xCFB11EAD453994BA,
    0x9ABE14CD44753B52, 0xE69594BEC44DE15B, 0xABCC77118461CEFC, 0x8000000000000000,
    0xBEBC200000000000, 0x8E1BC9BF04000000, 0xD3C21BCECCEDA100, 0x9DC5ADA82B70B59D,
};
static_assert(POW10_BY_8_FIRST == -7 && POW10_BY_8_LAST == 4 &&
                  sizeof(POW10_BY_8) / sizeof(POW10_BY_8[0]) == POW10_BY_8_LAST - POW10_BY_8_FIRST + 1,
              "POW10_BY_8 must cover MIN_E10..MAX_E10");

// floor(q * log2(10)), for |q| < 100
constexpr int32_t floor_log2_pow10(int32_t q) { return (q * 217706) >> 16; }

// The top 64 bits of a * b, rounded down
uint64_t mul_high(uint64_t a, uint64_t b) {
  const uint64_t a_lo = static_cast<uint32_t>(a);
  const uint64_t a_hi = a >> 32;
  const uint64_t b_lo = static_cast<uint32_t>(b);
  const uint64_t b_hi = b >> 32;
  const uint64_t hi_lo = a_hi * b_lo;
  const uint64_t lo_hi = a_lo * b_hi;
  const uint64_t middle = ((a_lo * b_lo) >> 32) + static_cast<uint32_t>(hi_lo) + static_cast<uint32_t>(lo_hi);
  return a_hi * b_hi + (hi_lo >> 32) + (lo_hi >> 32) + (middle >> 32);
}

// Out of line and cold: a decimal within 16 units of a float midpoint, which is an exact tie (one other, 6 units
// above it, keeps its float). The value is (m10 * 5^e10) * 2^e10, or (m10 / 5^-e10) * 2^e10 when 5^-e10 divides m10.
__attribute__((noinline, cold)) uint32_t tie_float_bits(uint32_t m10, int32_t e10, uint32_t bits) {
  if (e10 >= 0) {
    if (e10 <= 14)  // m10 * 5^14 stays below 2^64
      return round_to_float_bits(m10 * (pow10<uint64_t>(e10) >> e10), e10, false);
  } else if (e10 >= -13) {
    const auto pow5 = static_cast<uint32_t>(pow10<uint64_t>(-e10) >> -e10);
    if (m10 % pow5 == 0)
      return round_to_float_bits(m10 / pow5, e10, false);
  }
  return bits;
}

// The float nearest to m10 * 10^e10, for 0 < m10 <= 10^9 and MIN_E10 <= e10 <= MAX_E10, as picolibc's engine gives.
// m10 * 10^(e10 mod 8) is exact in 64 bits, and one multiplication by POW10_BY_8 leaves the product less than 4 units
// (of its top bit's 2^-63) below the decimal. All 85 billion (m10, e10) give picolibc's bits; outside the ties and
// one non-tie, no product lies within 34 units of a float midpoint.
uint32_t decimal_to_float_bits(uint32_t m10, int32_t e10) {
  // Both operands are exact floats, so the one float operation gives the nearest float
  const uint32_t power = e10 < 0 ? -e10 : e10;
  if (m10 < (1u << 24) && power <= 10) {
    const auto value = static_cast<float>(m10);
    // 10^10 is an exact float but not a uint32_t
    const float factor = power == 10 ? 1e10f : static_cast<float>(pow10<uint32_t>(power));
    return bit_cast<uint32_t>(e10 < 0 ? value / factor : value * factor);
  }
  const int32_t k8 = e10 >> 3;  // rounds down
  const uint64_t x = uint64_t{m10} * pow10<uint32_t>(e10 & 7);
  const auto zeros = static_cast<int32_t>(std::countl_zero(x));
  uint64_t m = x << zeros;
  int32_t e = -zeros;
  if (k8 == 0)
    return round_to_float_bits(m, e, false);  // exact
  m = mul_high(m, POW10_BY_8[k8 - POW10_BY_8_FIRST]);
  e += floor_log2_pow10(8 * k8) + 1;
  if ((m >> 63) == 0) {
    m <<= 1;
    e--;
  }
  const uint32_t bits = round_to_float_bits(m, e, false);
  // The bits below the float's last one, and the midpoint among them
  const int32_t shift = std::max<int32_t>(40, -149 - e);
  if (shift <= 64) {
    const uint64_t low = shift < 64 ? m & ((uint64_t{1} << shift) - 1) : m;
    const uint64_t half = uint64_t{1} << (shift - 1);
    if (low - half + 16 > 32)  // |low - half| > 16
      return bits;
  }
  return tie_float_bits(m10, e10, bits);
}

uint32_t no_number(const char *str, const char **end, bool *negative) {
  *end = str;
  *negative = false;
  return 0;
}

// The rest of an "nan" or "inf" after its first letter at p; returns the end, or nullptr for no number
const char *read_inf_nan(const char *p) {
  const auto lower = [](char c) { return static_cast<char>(c | 0x20); };
  if (lower(*p) == 'n') {
    if (lower(p[1]) != 'a' || lower(p[2]) != 'n')
      return nullptr;
    p += 3;
    if (*p != '(')
      return p;
    // "nan(" must be closed after letters, digits and '_'
    do {
      p++;
    } while (static_cast<uint8_t>(lower(*p) - 'a') < 26 || is_digit(*p) || *p == '_');
    return *p == ')' ? p + 1 : nullptr;
  }
  if (lower(p[1]) != 'n' || lower(p[2]) != 'f')
    return nullptr;
  p += 3;
  if (lower(*p) != 'i')
    return p;
  // Past "infi", only "infinity" is a number
  if (lower(p[1]) != 'n' || lower(p[2]) != 'i' || lower(p[3]) != 't' || lower(p[4]) != 'y')
    return nullptr;
  return p + 5;
}

// Reads a binary exponent "p" or decimal exponent "e" at p into *exp; returns the end of the number
const char *read_exponent(const char *p, char letter, int32_t *exp) {
  if ((*p | 0x20) != letter)
    return p;
  const char *q = p + 1;
  const bool negative = *q == '-';
  if (negative || *q == '+')
    q++;
  if (!is_digit(*q))
    return p;
  int32_t value = 0;
  for (; is_digit(*q); q++) {
    if (value < EXPONENT_CAP)
      value = value * 10 + (*q - '0');
  }
  *exp += negative ? -value : value;
  return q;
}

// Hex digits after the "x" at p, with the point and decimal exponent read so far (no non-zero digit yet)
__attribute__((noinline, cold)) uint32_t read_hex(const char *p, bool dot, int32_t exp, const char **end) {
  uint32_t m = 0;
  uint32_t kept = 0;  // digits in m from its first non-zero one
  bool sticky = false;
  for (;; p++) {
    const char lower = static_cast<char>(*p | 0x20);
    uint32_t digit = static_cast<uint8_t>(*p - '0');
    if (static_cast<uint8_t>(lower - 'a') <= 5)
      digit = static_cast<uint8_t>(lower - 'a') + 10;
    if (digit < 16) {
      if (kept < HEX_DIGITS) {
        if (dot)
          exp--;
        m = m * 16 + digit;
        kept += m != 0 ? 1 : 0;
      } else {
        if (!dot)
          exp++;
        sticky |= digit != 0;
      }
    } else if (*p == '.' && !dot) {
      dot = true;
    } else if (lower != 'x' || m != 0) {  // picolibc takes another "x" while the value is still zero
      break;
    }
  }
  exp *= 4;
  *end = read_exponent(p, 'p', &exp);
  return m == 0 ? 0 : round_to_float_bits(m, exp, sticky);
}

// picolibc's conv_flt for a float from the first digit or point at p: returns the bits; *end is str for no number
__attribute__((noinline)) uint32_t read_decimal(const char *str, const char *p, const char **end) {
  uint32_t m = 0;     // the first (up to 9) significant digits
  uint32_t kept = 0;  // digits in m from its first non-zero one
  int32_t exp = 0;    // the value is m * 10^exp
  bool dot = false;
  bool any = false;
  uint32_t overflow = 0;  // digits past the kept ones: 0 none, 1 below half, 2 half, 3 above half
  bool full = false;      // a digit past the kept ones was read
  for (;; p++) {
    const uint32_t digit = static_cast<uint8_t>(*p - '0');
    if (digit <= 9) {
      any = true;
      if (kept < DECIMAL_DIGITS) {
        if (dot)
          exp--;
        m = m * 10 + digit;
        kept += m != 0 ? 1 : 0;
      } else {
        if (!dot)
          exp++;
        if (!full) {
          full = true;
          overflow = digit > 5 ? 3 : digit == 5 ? 2 : digit != 0 ? 1 : 0;  // the first one decides the half
        } else {
          overflow |= digit != 0 ? 1 : 0;
        }
      }
    } else if (*p == '.' && !dot) {
      dot = true;
    } else {
      break;
    }
  }
  if ((*p | 0x20) == 'x' && any && m == 0)
    return read_hex(p + 1, dot, exp, end);
  if (!any) {
    *end = str;
    return 0;
  }
  *end = read_exponent(p, 'e', &exp);
  if (m == 0)
    return 0;
  // Ties to even on the decimal digits
  overflow |= m & 1;
  m += (overflow + 1) >> 2;
  const int32_t exp10 = static_cast<int32_t>(kept) + exp;
  if (exp10 <= MIN_EXP10)
    return 0;
  if (exp10 >= MAX_EXP10)
    return FLOAT_INF_BITS;
  return decimal_to_float_bits(m, exp);
}

// picolibc's strtof from the start: returns the bits without the sign, which goes in *negative
__attribute__((noinline)) uint32_t parse_float_full(const char *str, const char **end, bool *negative) {
  // No number starts with a letter but i (inf) or n (nan): "unavailable", "on" and "unknown" stop here
  const char first = static_cast<char>(*str | 0x20);
  if (first >= 'a' && first <= 'z' && first != 'i' && first != 'n')
    return no_number(str, end, negative);
  const char *p = str;
  while (is_space_c(*p))
    p++;
  *negative = *p == '-';
  if (*negative || *p == '+')
    p++;
  const char lower = static_cast<char>(*p | 0x20);
  if (lower == 'i' || lower == 'n') {
    const char *stop = read_inf_nan(p);
    if (stop == nullptr)
      return no_number(str, end, negative);
    *end = stop;
    return lower == 'i' ? FLOAT_INF_BITS : FLOAT_NAN_BITS;
  }
  const uint32_t bits = read_decimal(str, p, end);
  if (*end == str)
    *negative = false;
  return bits;
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
    if (mantissa > PLAIN_MANTISSA_LIMIT)
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

optional<float> parse_float(const char *str) {
  // Its own loop for the whole string as a plain decimal (below 2^24 and up to 9 digits after the point): sharing
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
    if (mantissa > PLAIN_MANTISSA_LIMIT)
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
