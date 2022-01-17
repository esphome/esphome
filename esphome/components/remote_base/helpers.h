#pragma once
#include "remote_base.h"

namespace esphome {
namespace remote_base {

/// Template helper class for data coded by space
template<uint32_t mark_us, uint32_t space_one_us, uint32_t space_zero_us> class space {
 public:
  /// Helper class for data coded by space in MSB bit order
  class msb {
   public:
    /// Encode data by space in MSB bit order
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        dst->item(mark_us, (src & mask) ? space_one_us : space_zero_us);
    }
    /// Decode data by space in MSB bit order. Return number of decoded bits.
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      size_t bit = 0;
      for (; bit != nbits; bit++) {
        if (!src.expect_mark(mark_us))
          break;
        if (src.expect_space(space_one_us))
          dst = (dst << 1) | 1;
        else if (src.expect_space(space_zero_us))
          dst = (dst << 1) | 0;
        else
          break;
      }
      return bit;
    }
    /// Decode and compare data by space in MSB bit order. Return true if data equals.
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        if (!src.expect_item(mark_us, (data & mask) ? space_one_us : space_zero_us))
          return false;
      return true;
    }
    /// Inverse functions
    class inv {
     public:
      /// Inverted encode data by space in MSB bit order
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
          dst->item(mark_us, (src & mask) ? space_zero_us : space_one_us);
      }
      /// Inverted decode data by space in MSB bit order. Return number of decoded bits.
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        size_t bit = 0;
        for (; bit != nbits; dst <<= 1, bit++) {
          if (!src.expect_mark(mark_us))
            break;
          if (src.expect_space(space_zero_us))
            dst |= 1;
          else if (!src.expect_space(space_one_us))
            break;
        }
        return bit;
      }
      /// Inverted decode and compare data by space in MSB bit order. Return true if data equals.
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
  class lsb {
   public:
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
        dst->item(mark_us, (src & mask) ? space_one_us : space_zero_us);
    }
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      const T mask = static_cast<T>(1ULL << (sizeof(T) * 8 - 1));
      T data = 0;
      size_t bit = 0;
      for (; bit != nbits; bit++) {
        if (!src.expect_mark(mark_us))
          break;
        if (src.expect_space(space_one_us))
          data = (data >> 1) | mask;
        else if (src.expect_space(space_zero_us))
          data = (data >> 1) | 0;
        else
          break;
      }
      if (bit > 0)
        dst = (dst << bit) | (data >> (sizeof(T) * 8 - bit));
      return bit;
    }
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
        if (!src.expect_item(mark_us, (data & mask) ? space_one_us : space_zero_us))
          return false;
      return true;
    }
    class inv {
     public:
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = 1 << 0; mask != static_cast<T>(1ULL << nbits); mask <<= 1)
          dst->item(mark_us, (src & mask) ? space_zero_us : space_one_us);
      }
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        const T mask = static_cast<T>(1ULL << (sizeof(T) * 8 - 1));
        T data = 0;
        size_t bit = 0;
        for (; bit != nbits; bit++) {
          if (!src.expect_mark(mark_us))
            break;
          if (src.expect_space(space_zero_us))
            data = (data >> 1) | mask;
          else if (src.expect_space(space_one_us))
            data = (data >> 1) | 0;
          else
            break;
        }
        if (bit > 0)
          dst = (dst << bit) | (data >> (sizeof(T) * 8 - bit));
        return bit;
      }
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

template<uint32_t space_us, uint32_t mark_one_us, uint32_t mark_zero_us> class mark {
 public:
  class msb {
   public:
    template<typename T> static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        dst->item((src & mask) ? mark_one_us : mark_zero_us, space_us);
    }
    template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      size_t bit = 0;
      for (; bit != nbits; dst <<= 1, bit++) {
        if (src.expect_mark(mark_one_us))
          dst |= 1;
        else if (!src.expect_mark(mark_zero_us))
          break;
        if (!src.expect_space(space_us))
          break;
      }
      return bit;
    }
    template<typename T> static bool equal(RemoteReceiveData &src, const T &data, const size_t nbits = sizeof(T) * 8) {
      static_assert(std::is_integral<T>::value, "T must be an integer.");
      for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
        if (!src.expect_item((data & mask) ? mark_one_us : mark_zero_us, space_us))
          return false;
      return true;
    }
    class inv {
     public:
      template<typename T>
      static void encode(RemoteTransmitData *dst, const T &src, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        for (T mask = static_cast<T>(1ULL << (nbits - 1)); mask != 0; mask >>= 1)
          dst->item((src & mask) ? mark_zero_us : mark_one_us, space_us);
      }
      template<typename T> static size_t decode(RemoteReceiveData &src, T &dst, const size_t nbits = sizeof(T) * 8) {
        static_assert(std::is_integral<T>::value, "T must be an integer.");
        size_t bit = 0;
        for (; bit != nbits; dst <<= 1, bit++) {
          if (src.expect_mark(mark_zero_us))
            dst |= 1;
          else if (!src.expect_mark(mark_one_us))
            break;
          if (!src.expect_space(space_us))
            break;
        }
        return bit;
      }
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
};

#define USE_SPACE_CODEC(name) using name = space<BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>;
#define USE_SPACE_MSB_CODEC(name) using name = space<BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>::msb;
#define USE_SPACE_LSB_CODEC(name) using name = space<BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>::lsb;

#define USE_MARK_CODEC(name) using name = mark<BIT_SPACE_US, BIT_ONE_MARK_US, BIT_ZERO_MARK_US>;
#define USE_MARK_MSB_CODEC(name) using name = mark<BIT_SPACE_US, BIT_ONE_MARK_US, BIT_ZERO_MARK_US>::msb;
#define USE_MARK_LSB_CODEC(name) using name = mark<BIT_SPACE_US, BIT_ONE_MARK_US, BIT_ZERO_MARK_US>::lsb;

}  // namespace remote_base
}  // namespace esphome
