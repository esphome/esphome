#pragma once
#include "remote_base.h"

namespace esphome {
namespace remote_base {

/// Template helper class for space-encoded data.
template<uint32_t mark_us, uint32_t space_one_us, uint32_t space_zero_us> class space {  // NOLINT
 public:
  /// Template helper class for space-encoded data with MSB bit order.
  class msb {  // NOLINT
   public:
    /// Encode data by space with MSB bit order.
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        dst->item(mark_us, (src & mask) ? space_one_us : space_zero_us);
    }
    /// Decodes space-encoded data in MSB bit order. The data is shifted left by the specified number of bits.
    /// Returns the number of decoded bits.
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      size_t cnt = 0;
      for (; cnt != nbits; cnt++) {
        if (!src.expect_mark(mark_us))
          break;
        if (src.expect_space(space_one_us))
          dst = (dst << 1) | 1;
        else if (src.expect_space(space_zero_us))
          dst = (dst << 1) | 0;
        else
          break;
      }
      return cnt;
    }
    /// Decodes and compares space-encoded data in MSB bit order.
    /// Returns true if data equals.
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        if (!src.expect_item(mark_us, (data & mask) ? space_one_us : space_zero_us))
          return false;
      return true;
    }
    /// Inner class for inverse template functions
    class inv {  // NOLINT
     public:
      /// Inverse encode data by space with MSB bit order.
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
          dst->item(mark_us, (src & mask) ? space_zero_us : space_one_us);
      }
      /// Decodes inverted space-encoded data in MSB bit order. The data is shifted left by the specified number of
      /// bits. Returns the number of decoded bits.
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        size_t cnt = 0;
        for (; cnt != nbits; cnt++) {
          if (!src.expect_mark(mark_us))
            break;
          if (src.expect_space(space_zero_us))
            dst = (dst << 1) | 1;
          else if (src.expect_space(space_one_us))
            dst = (dst << 1) | 0;
          else
            break;
        }
        return cnt;
      }
      /// Decodes and compares inverted space-encoded data in MSB bit order.
      /// Returns true if data equals.
      template<typename T>
      static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
          if (!src.expect_item(mark_us, (data & mask) ? space_zero_us : space_one_us))
            return false;
        return true;
      }
    };
  };
  class lsb {  // NOLINT
   public:
    /// Encode data by space with LSB bit order.
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
        dst->item(mark_us, (src & mask) ? space_one_us : space_zero_us);
    }
    /// Decodes space-encoded data in LSB bit order. The data is shifted left by the specified number of bits.
    /// Returns the number of decoded bits.
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      constexpr size_t max_bits = sizeof(T) * 8;
      constexpr T mask = static_cast<T>(1ULL << (max_bits - 1));
      T data = 0;
      size_t cnt = 0;
      for (; cnt != nbits; cnt++) {
        if (!src.expect_mark(mark_us))
          break;
        if (src.expect_space(space_one_us))
          data = (data >> 1) | mask;
        else if (src.expect_space(space_zero_us))
          data = (data >> 1) | 0;
        else
          break;
      }
      if (cnt != 0)
        dst = (dst << cnt) | (data >> (max_bits - cnt));
      return cnt;
    }
    /// Decodes and compares space-encoded data in LSB bit order.
    /// Returns true if data equals.
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
        if (!src.expect_item(mark_us, (data & mask) ? space_one_us : space_zero_us))
          return false;
      return true;
    }
    /// Inner class for inverse template functions
    class inv {  // NOLINT
     public:
      /// Inverse encode data by space with LSB bit order.
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
          dst->item(mark_us, (src & mask) ? space_zero_us : space_one_us);
      }
      /// Decodes inverted space-encoded data in LSB bit order. The data is shifted left by the specified number of
      /// bits. Returns the number of decoded bits.
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        constexpr size_t max_bits = sizeof(T) * 8;
        constexpr T mask = static_cast<T>(1ULL << (max_bits - 1));
        T data = 0;
        size_t cnt = 0;
        for (; cnt != nbits; cnt++) {
          if (!src.expect_mark(mark_us))
            break;
          if (src.expect_space(space_zero_us))
            data = (data >> 1) | mask;
          else if (src.expect_space(space_one_us))
            data = (data >> 1) | 0;
          else
            break;
        }
        if (cnt != 0)
          dst = (dst << cnt) | (data >> (max_bits - cnt));
        return cnt;
      }
      /// Decodes and compares inverted space-encoded data in LSB bit order.
      /// Returns true if data equals.
      template<typename T>
      static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
          if (!src.expect_item(mark_us, (data & mask) ? space_zero_us : space_one_us))
            return false;
        return true;
      }
    };
  };
};

/// Template helper class for mark-encoded data.
template<uint32_t space_us, uint32_t mark_one_us, uint32_t mark_zero_us> class mark {  // NOLINT
 public:
  /// Template helper class for mark-encoded data with MSB bit order.
  class msb {  // NOLINT
   public:
    /// Encode data by mark with MSB bit order.
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        dst->item((src & mask) ? mark_one_us : mark_zero_us, space_us);
    }
    /// Decodes mark-encoded data in MSB bit order. The data is shifted left by the specified number of bits.
    /// Returns the number of decoded bits.
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      size_t cnt = 0;
      for (; cnt != nbits; cnt++) {
        if (src.expect_mark(mark_one_us))
          dst = (dst << 1) | 1;
        else if (src.expect_mark(mark_zero_us))
          dst = (dst << 1) | 0;
        else
          break;
        if (!src.expect_space(space_us))
          break;
      }
      return cnt;
    }
    /// Decodes and compares mark-encoded data in MSB bit order.
    /// Returns true if data equals.
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        if (!src.expect_item((data & mask) ? mark_one_us : mark_zero_us, space_us))
          return false;
      return true;
    }
    /// Inner class for inverse template functions
    class inv {  // NOLINT
     public:
      /// Inverse encode data by mark with MSB bit order.
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
          dst->item((src & mask) ? mark_zero_us : mark_one_us, space_us);
      }
      /// Decodes inverted mark-encoded data in MSB bit order. The data is shifted left by the specified number of bits.
      /// Returns the number of decoded bits.
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        size_t cnt = 0;
        for (; cnt != nbits; cnt++) {
          if (src.expect_mark(mark_zero_us))
            dst = (dst << 1) | 1;
          else if (src.expect_mark(mark_one_us))
            dst = (dst << 1) | 0;
          else
            break;
          if (!src.expect_space(space_us))
            break;
        }
        return cnt;
      }
      /// Decodes and compares inverted mark-encoded data in MSB bit order.
      /// Returns true if data equals.
      template<typename T>
      static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
          if (!src.expect_item((data & mask) ? mark_zero_us : mark_one_us, space_us))
            return false;
        return true;
      }
    };
  };
  class lsb {  // NOLINT
   public:
    /// Encode data by mark with LSB bit order.
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
        dst->item((src & mask) ? mark_one_us : mark_zero_us, space_us);
    }
    /// Decodes mark-encoded data in LSB bit order. The data is shifted left by the specified number of bits.
    /// Returns the number of decoded bits.
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      constexpr size_t max_bits = sizeof(T) * 8;
      constexpr T mask = static_cast<T>(1ULL << (max_bits - 1));
      T data = 0;
      size_t cnt = 0;
      for (; cnt != nbits; cnt++) {
        if (src.expect_mark(mark_one_us))
          data = (data >> 1) | mask;
        else if (src.expect_mark(mark_zero_us))
          data = (data >> 1) | 0;
        else
          break;
        if (!src.expect_space(space_us))
          break;
      }
      if (cnt != 0)
        dst = (dst << cnt) | (data >> (max_bits - cnt));
      return cnt;
    }
    /// Decodes and compares mark-encoded data in LSB bit order.
    /// Returns true if data equals.
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
        if (!src.expect_item((data & mask) ? mark_one_us : mark_zero_us, space_us))
          return false;
      return true;
    }
    /// Inner class for inverse template functions
    class inv {  // NOLINT
     public:
      /// Inverse encode data by mark with LSB bit order.
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
          dst->item((src & mask) ? mark_zero_us : mark_one_us, space_us);
      }
      /// Decodes inverted mark-encoded data in LSB bit order. The data is shifted left by the specified number of bits.
      /// Returns the number of decoded bits.
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        constexpr size_t max_bits = sizeof(T) * 8;
        constexpr T mask = static_cast<T>(1ULL << (max_bits - 1));
        T data = 0;
        size_t cnt = 0;
        for (; cnt != nbits; cnt++) {
          if (src.expect_mark(mark_zero_us))
            data = (data >> 1) | mask;
          else if (src.expect_mark(mark_one_us))
            data = (data >> 1) | 0;
          else
            break;
          if (!src.expect_space(space_us))
            break;
        }
        if (cnt != 0)
          dst = (dst << cnt) | (data >> (max_bits - cnt));
        return cnt;
      }
      /// Decodes and compares inverted mark-encoded data in LSB bit order.
      /// Returns true if data equals.
      template<typename T>
      static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
          if (!src.expect_item((data & mask) ? mark_zero_us : mark_one_us, space_us))
            return false;
        return true;
      }
    };
  };
};

// NOLINTBEGIN
#define USE_SPACE_CODEC(name) using name = space<BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>;
#define USE_SPACE_MSB_CODEC(name) using name = space<BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>::msb;
#define USE_SPACE_LSB_CODEC(name) using name = space<BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>::lsb;
#define USE_MARK_CODEC(name) using name = mark<BIT_SPACE_US, BIT_ONE_MARK_US, BIT_ZERO_MARK_US>;
#define USE_MARK_MSB_CODEC(name) using name = mark<BIT_SPACE_US, BIT_ONE_MARK_US, BIT_ZERO_MARK_US>::msb;
#define USE_MARK_LSB_CODEC(name) using name = mark<BIT_SPACE_US, BIT_ONE_MARK_US, BIT_ZERO_MARK_US>::lsb;
// NOLINTEND

}  // namespace remote_base
}  // namespace esphome
