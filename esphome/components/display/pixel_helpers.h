#pragma once

#include <cstdarg>
#include <vector>

#include "pixel_formats.h"

#include "esphome/core/color.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

template<int In, int Out>
inline ALWAYS_INLINE uint8_t shift_bits(uint8_t src) {
  if (!In || !Out) {
    return 0;
#if 1
  } else if (In == 1 && In < Out) {
    // expand: fast expand
    //return src ? (0xFF >> (8 - Out)) : 0x00;
    return -src >> (8 - Out);
#endif
#if 1
  } else if (In < Out) {
    return src * ((255 / ((1<<In)-1)) >> (8 - Out));
#else
  } else if (In == 2 && In < Out) {
    // expand: fast expand
    uint8_t out = 0;
    // out |= src & 0x02 ? (0xAA >> (8 - Out)) : 0;
    // out |= src & 0x01 ? (0x55 >> (8 - Out)) : 0;
    out = src * (0x55 >> (8 - Out));
    return out;
  } else if (In < Out) {
    // expand: In=5 => Out=8
    // src << (8-5) + src >> (5 - (8-5))
    // src << (8-5) + src >> (5 - 8 + 5)
    uint8_t out = src << (Out-In);

    for (int i = 2; i < 8; i++) {
      if (Out-i*In >= 0)
        out += src << (Out-i*In);
      else if (Out-i*In >= -In)
        out += src >> -(Out-i*In);
      else
        break;
    }

    return out;
#endif
  } else if (In > Out) {
    // reduce: In=8 => Out=5
    // (src + (1<<(8-5) - 1)) >> (8-5)
    return src >> (In-Out);
  } else {
    return src;
  }
}

template<typename Out, typename In>
struct PixelConverter
{
  static inline ALWAYS_INLINE Out &convert(Out &out, int out_packed_pixel, const In &in, int in_packed_pixel = 0) {
    uint8_t r, g, b, a, w;
    in.decode(r, g, b, a, w, in_packed_pixel);

    uint8_t approx_w = shift_bits<In::R, 6>(r) + shift_bits<In::G, 7>(g) + shift_bits<In::B, 6>(b);

    out.encode(
      In::R ? shift_bits<In::R, Out::R>(r) : In::W ? shift_bits<In::W, Out::R>(w) : shift_bits<In::A, Out::R>(a),
      In::G ? shift_bits<In::G, Out::G>(g) : In::W ? shift_bits<In::W, Out::G>(w) : shift_bits<In::A, Out::G>(a),
      In::B ? shift_bits<In::B, Out::B>(b) : In::W ? shift_bits<In::W, Out::B>(w) : shift_bits<In::A, Out::B>(a),
      In::A ? shift_bits<In::A, Out::A>(a) : shift_bits<In::W, Out::A>(w),
      In::W ? shift_bits<In::W, Out::W>(w) : shift_bits<8, Out::W>(approx_w),
      out_packed_pixel
    );
    return out;
  }
};

template<PixelFormat OutFormat, bool OutBigEndian, PixelFormat InFormat, bool InBigEndian>
struct PixelConverter<PixelRGB565_Endiness<OutFormat, OutBigEndian>, PixelRGB565_Endiness<InFormat, InBigEndian> >
{
  typedef PixelRGB565_Endiness<OutFormat, OutBigEndian> Out;
  typedef PixelRGB565_Endiness<InFormat, InBigEndian> In;

  static inline ALWAYS_INLINE Out &convert(Out &out, int out_packed_pixel, const In &in, int in_packed_pixel = 0) {
    out.raw_16 = OutBigEndian == InBigEndian ? in.raw_16 : byteswap(in.raw_16);
    return out;
  }
};

template<typename Out>
static inline ALWAYS_INLINE Out from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
  Out out;
  const uint8_t approx_w = (r >> 2) + (g >> 1) + (b >> 2);
  out.encode(
    shift_bits<8, Out::R>(r),
    shift_bits<8, Out::G>(g),
    shift_bits<8, Out::B>(b),
    shift_bits<8, Out::A>(a),
    shift_bits<8, Out::W>(approx_w)
  );
  return out;
}

template<typename Out>
static inline ALWAYS_INLINE Out from_w(uint8_t w, uint8_t a = 0xFF) {
  Out out;
  out.encode(
    shift_bits<8, Out::R>(w),
    shift_bits<8, Out::G>(w),
    shift_bits<8, Out::B>(w),
    shift_bits<8, Out::A>(a),
    shift_bits<8, Out::W>(w)
  );
  return out;
}

template<typename Out>
static inline ALWAYS_INLINE Out from_color(Out &out, const Color &in, int out_pixel = 0) {
  const uint8_t approx_w = (in.r >> 2) + (in.g >> 1) + (in.b >> 2);
  out.encode(
    shift_bits<8, Out::R>(in.r),
    shift_bits<8, Out::G>(in.g),
    shift_bits<8, Out::B>(in.b),
    shift_bits<8, Out::A>(in.w),
    shift_bits<8, Out::W>(approx_w),
    out_pixel
  );
  return out;
}

template<typename Out>
static inline ALWAYS_INLINE Out from_color(const Color &in, bool expand_packed = true) {
  Out out;
  from_color(out, in);
  if (expand_packed) {
    for (int i = 1; i < Out::PACKED_PIXELS; i++) {
      from_pixel_format(out, i, out, 0);
    }
  }
  return out;
}

template<typename In>
static inline ALWAYS_INLINE Color to_color(const In &in, int pixel = 0) {
  uint8_t r, g, b, a, w;
  in.decode(r, g, b, a, w, pixel);

  return Color(
    In::R ? r : In::W ? w : a,
    In::G ? g : In::W ? w : a,
    In::B ? b : In::W ? w : a,
    In::W ? w : In::A ? a : 0xFF
  );
}

template<typename Out>
static inline ALWAYS_INLINE Out *offset_buffer(Out *buffer, int x, int y, int width) {
  return &buffer[y * Out::array_stride(width) + Out::array_offset(x)];
}

template<typename Out>
static inline ALWAYS_INLINE Out *offset_buffer(Out *buffer, int x) {
  return &buffer[Out::array_offset(x)];
}

template<typename Out>
static inline ALWAYS_INLINE Out *offset_end_buffer(Out *buffer, int x) {
  return &buffer[Out::array_stride(x)];
}

template<typename Out, typename In>
static inline ALWAYS_INLINE Out &from_pixel_format(Out &out, int out_packed_pixel, const In &in, int in_packed_pixel = 0) {
  return PixelConverter<Out, In>::convert(out, out_packed_pixel, in, in_packed_pixel);
};

template<typename Out, typename In>
static inline ALWAYS_INLINE Out from_pixel_format(const In &in, int in_packed_pixel = 0) {
  Out out;
  return from_pixel_format(out, 0, in, in_packed_pixel);
}

template<typename Out>
static inline ALWAYS_INLINE Out &copy_pixel(Out &out, const Out &in, int start_packed_pixel = 0, int end_packed_pixel = Out::PACKED_PIXELS) {
  for (int i = start_packed_pixel; i < end_packed_pixel; i++) {
    from_pixel_format(out, i, in, i);
  }
  return out;
}

} // display
} // esphome
