#pragma once

#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome {

namespace internal {

/// Wrapper class for memory using big endian data layout, transparently converting it to native order.
template<typename T> class BigEndianLayout {
 public:
  constexpr14 operator T() { return convert_big_endian(val_); }

 private:
  T val_;
} __attribute__((packed));

/// Wrapper class for memory using big endian data layout, transparently converting it to native order.
template<typename T> class LittleEndianLayout {
 public:
  constexpr14 operator T() { return convert_little_endian(val_); }

 private:
  T val_;
} __attribute__((packed));

}  // namespace internal

/// 24-bit unsigned integer type, transparently converting to 32-bit.
struct uint24_t {  // NOLINT(readability-identifier-naming)
  operator uint32_t() { return val; }
  uint32_t val : 24;
} __attribute__((packed));

/// 24-bit signed integer type, transparently converting to 32-bit.
struct int24_t {  // NOLINT(readability-identifier-naming)
  operator int32_t() { return val; }
  int32_t val : 24;
} __attribute__((packed));

// Integer types in big or little endian data layout.
using uint64_be_t = internal::BigEndianLayout<uint64_t>;
using uint32_be_t = internal::BigEndianLayout<uint32_t>;
using uint24_be_t = internal::BigEndianLayout<uint24_t>;
using uint16_be_t = internal::BigEndianLayout<uint16_t>;
using int64_be_t = internal::BigEndianLayout<int64_t>;
using int32_be_t = internal::BigEndianLayout<int32_t>;
using int24_be_t = internal::BigEndianLayout<int24_t>;
using int16_be_t = internal::BigEndianLayout<int16_t>;
using uint64_le_t = internal::LittleEndianLayout<uint64_t>;
using uint32_le_t = internal::LittleEndianLayout<uint32_t>;
using uint24_le_t = internal::LittleEndianLayout<uint24_t>;
using uint16_le_t = internal::LittleEndianLayout<uint16_t>;
using int64_le_t = internal::LittleEndianLayout<int64_t>;
using int32_le_t = internal::LittleEndianLayout<int32_t>;
using int24_le_t = internal::LittleEndianLayout<int24_t>;
using int16_le_t = internal::LittleEndianLayout<int16_t>;

}  // namespace esphome
