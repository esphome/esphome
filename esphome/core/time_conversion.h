#pragma once

#include <cstdint>

namespace esphome {

/// Convert a 64-bit microsecond count to milliseconds without calling
/// __udivdi3 (software 64-bit divide, ~1200 ns on Xtensa @ 240 MHz).
///
/// Returns uint32_t by default (for millis()), or uint64_t when requested
/// (for millis_64()). The only difference is whether hi * Q is truncated
/// to 32 bits or widened to 64.
///
/// On 32-bit targets, GCC does not optimize 64-bit constant division into a
/// multiply-by-reciprocal. Since 1000 = 8 * 125, we first right-shift by 3
/// (free divide-by-8), then use the Euclidean division identity to decompose
/// the remaining 64-bit divide-by-125 into a single 32-bit division:
///
///   floor(us / 1000) = floor(floor(us / 8) / 125)    [exact for integers]
///   2^32 = Q * 125 + R  (34359738 * 125 + 46)
///   (hi * 2^32 + lo) / 125 = hi * Q + (hi * R + lo) / 125
///
/// GCC optimizes the remaining 32-bit "/ 125U" into a multiply-by-reciprocal
/// (mulhu + shift), so no division instruction is emitted.
///
/// Safe for us up to ~3.2e18 (~101,700 years of microseconds).
///
/// See: https://en.wikipedia.org/wiki/Euclidean_division
/// See: https://ridiculousfish.com/blog/posts/labor-of-division-episode-iii.html
template<typename ReturnT = uint32_t>
__attribute__((always_inline)) inline constexpr ReturnT micros_to_millis(uint64_t us) {
  constexpr uint32_t d = 125U;
  constexpr uint32_t q = static_cast<uint32_t>((1ULL << 32) / d);  // 34359738
  constexpr uint32_t r = static_cast<uint32_t>((1ULL << 32) % d);  // 46
  // 1000 = 8 * 125; divide-by-8 is a free shift
  uint64_t x = us >> 3;
  uint32_t lo = static_cast<uint32_t>(x);
  uint32_t hi = static_cast<uint32_t>(x >> 32);
  // Combine remainder term: hi * (2^32 % 125) + lo
  uint32_t adj = hi * r + lo;
  // If adj overflowed, the true value is 2^32 + adj; apply the identity again
  // static_cast<ReturnT>(hi) widens to 64-bit when ReturnT=uint64_t, preserving upper bits of hi*q
  return static_cast<ReturnT>(hi) * q + (adj < lo ? (adj + r) / d + q : adj / d);
}

}  // namespace esphome
