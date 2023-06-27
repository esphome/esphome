#include "pixel_bitblt.h"
#include "pixel_helpers.h"

#include <utility>

namespace esphome {
namespace display {

template<typename PixelFormat>
inline ALWAYS_INLINE void bitblt_copy(
  PixelFormat *dest_p, const PixelFormat *src_p, int p_pixel, int width)
{
  auto dest_end_p = offset_end_buffer(dest_p, p_pixel + width);
  if (dest_p == dest_end_p)
    return;
  auto src_end_p = offset_end_buffer(src_p, p_pixel + width);

  // copy starting pixels
  if (p_pixel > 0) {
    copy_pixel(*dest_p++, *src_p++, p_pixel);
    if (dest_p == dest_end_p)
      return;
  }

  // copy ending pixels
  int end_packed_pixels = PixelFormat::packed_pixel(p_pixel + width);
  if (end_packed_pixels > 0) {
    copy_pixel(*--dest_end_p, *--src_end_p, 0, end_packed_pixels);
    if (dest_p == dest_end_p)
      return;
  }

  memcpy(dest_p, src_p, (dest_end_p - dest_p) * PixelFormat::BYTES);
}

template<bool Transparency, typename SrcPixelFormat, typename DestPixelFormat>
inline ALWAYS_INLINE void bitblt_fast(
  DestPixelFormat *dest_p, const SrcPixelFormat *src_p, int width,
  DestPixelFormat color_on, DestPixelFormat color_off)
{
  auto dest_end_p = offset_end_buffer(dest_p, width);

  for ( ; dest_p < dest_end_p; dest_p++, src_p++) {
    if (Transparency && src_p->is_transparent(0)) {
      continue;
    }

    if (SrcPixelFormat::COLOR_KEY) {
      if (src_p->is_on(0))
        *dest_p = color_on;
      else
        *dest_p = color_off;
    } else {
      from_pixel_format(*dest_p, 0, *src_p, 0);
    }
  }
}

template<bool Transparency, typename SrcPixelFormat, typename DestPixelFormat>
inline ALWAYS_INLINE void bitblt_semi_fast_src_packed_pixels(
  DestPixelFormat *dest_p, const SrcPixelFormat *src_p, int src_p_pixel, int width,
  DestPixelFormat color_on, DestPixelFormat color_off)
{
  auto dest_end_p = offset_end_buffer(dest_p, width);

  for ( ; dest_p < dest_end_p; src_p++, src_p_pixel = 0) {
    for ( ; src_p_pixel < SrcPixelFormat::PACKED_PIXELS && dest_p < dest_end_p; src_p_pixel++, dest_p++) {
      if (Transparency && src_p->is_transparent(src_p_pixel)) {
        continue;
      }

      if (SrcPixelFormat::COLOR_KEY) {
        if (src_p->is_on(src_p_pixel))
          *dest_p = color_on;
        else
          *dest_p = color_off;
      } else {
        from_pixel_format(*dest_p, 0, *src_p, src_p_pixel);
      }
    }
  }
}

template<bool Transparency, typename SrcPixelFormat, typename DestPixelFormat>
inline ALWAYS_INLINE void bitblt_semi_fast_dest_packed_pixels(
  DestPixelFormat *dest_p, int dest_p_pixel, const SrcPixelFormat *src_p, int width,
  DestPixelFormat color_on, DestPixelFormat color_off)
{
  auto src_end_p = offset_end_buffer(src_p, width);

  for ( ; src_p < src_end_p; dest_p++, dest_p_pixel = 0) {
    for ( ; dest_p_pixel < DestPixelFormat::PACKED_PIXELS && src_p < src_end_p; dest_p_pixel++, src_p++) {
      if (Transparency && src_p->is_transparent(0)) {
        continue;
      }

      if (SrcPixelFormat::COLOR_KEY) {
        if (src_p->is_on(0))
          from_pixel_format(*dest_p, dest_p_pixel, color_on);
        else
          from_pixel_format(*dest_p, dest_p_pixel, color_off);
      } else {
        from_pixel_format(*dest_p, dest_p_pixel, *src_p);
      }
    }
  }
}

template<bool Transparency, typename SrcPixelFormat, typename DestPixelFormat>
inline ALWAYS_INLINE void bitblt_slow_src_dest_packed_pixels(
  DestPixelFormat *dest_p, int dest_p_pixel,
  const SrcPixelFormat *src_p, int src_p_pixel, int width,
  DestPixelFormat color_on, DestPixelFormat color_off)
{
  for (int pixels = 0; pixels < width; src_p++, src_p_pixel = 0) {
    for ( ; src_p_pixel < DestPixelFormat::PACKED_PIXELS && pixels < width; src_p_pixel++, dest_p_pixel++, pixels++) {
      assert(dest_p_pixel <= DestPixelFormat::PACKED_PIXELS);

      if (dest_p_pixel == DestPixelFormat::PACKED_PIXELS) {
        dest_p++;
        dest_p_pixel = 0;
      }

      if (Transparency && src_p->is_transparent(src_p_pixel)) {
        continue;
      }

      if (SrcPixelFormat::COLOR_KEY) {
        if (src_p->is_on(src_p_pixel))
          from_pixel_format(*dest_p, dest_p_pixel, color_on);
        else
          from_pixel_format(*dest_p, dest_p_pixel, color_off);
      } else {
        from_pixel_format(*dest_p, dest_p_pixel, *src_p, src_p_pixel);
      }
    }
  }
}

template<typename SrcPixelFormat, typename DestPixelFormat, bool Transparency>
void bitblt(DestPixelFormat *dest, int dest_x, const SrcPixelFormat *src, int src_x, int width, DestPixelFormat color_on, DestPixelFormat color_off)
{
  auto dest_p = offset_buffer(dest, dest_x);
  auto src_p = offset_buffer(src, src_x);

  if (SrcPixelFormat::FORMAT == DestPixelFormat::FORMAT && DestPixelFormat::packed_pixel(dest_x) == SrcPixelFormat::packed_pixel(src_x)) {
    bitblt_copy(dest_p, (DestPixelFormat*)src_p, SrcPixelFormat::packed_pixel(src_x), width);
  } else if (SrcPixelFormat::PACKED_PIXELS == 1 && DestPixelFormat::PACKED_PIXELS == 1) {
    bitblt_fast<Transparency>(dest_p, src_p, width, color_on, color_off);
  } else if (SrcPixelFormat::PACKED_PIXELS != 1) {
    bitblt_semi_fast_src_packed_pixels<Transparency>(dest_p, src_p, SrcPixelFormat::packed_pixel(src_x), width, color_on, color_off);
  } else if (DestPixelFormat::PACKED_PIXELS != 1) {
    bitblt_semi_fast_dest_packed_pixels<Transparency>(dest_p, DestPixelFormat::packed_pixel(dest_x), src_p, width, color_on, color_off);
  } else {
    bitblt_slow_src_dest_packed_pixels<Transparency>(dest_p, DestPixelFormat::packed_pixel(dest_x), src_p, SrcPixelFormat::packed_pixel(src_x), width, color_on, color_off);
  }
}

template<typename PixelFormat>
void fill(PixelFormat *dest, int x, int width, const PixelFormat &color)
{
  auto destp = offset_buffer(dest, x);
  auto endp = offset_end_buffer(dest, x + width);
  if (destp == endp)
    return;

  // handle packed pixels
  if (PixelFormat::PACKED_PIXELS > 1) {
    auto packed_pixel = PixelFormat::packed_pixel(x);

    // copy start pixels
    if (packed_pixel > 0) {
      copy_pixel(*destp, color, packed_pixel);
      destp++;
    }

    // copy end pixels
    auto end_packed_pixel = PixelFormat::packed_pixel(x + width);
    if (end_packed_pixel > 0 && destp < endp) {
      --endp;
      copy_pixel(*endp, color, 0, end_packed_pixel);
    }
  }

  for (auto p = destp; p < endp; p++)
    *p = color;
}

#define EXPORT_IGNORE(...)
#define EXPORT_SRC_DEST_BITBLT(src_format, dest_format, ...) \
  template void bitblt<Pixel##src_format, Pixel##dest_format, ##__VA_ARGS__>( \
    Pixel##dest_format *dest, int dest_x, \
    const Pixel##src_format *src, int src_x, int width, \
    Pixel##dest_format color_on, Pixel##dest_format color_off)

#define EXPORT_DEST_BITBLT(src_format, dest_format) \
  EXPORT_SRC_DEST_BITBLT(src_format, dest_format, false); \
  EXPORT_SRC_DEST_BITBLT(src_format, dest_format, true)

#define EXPORT_FILL(dest_format) \
  template void fill( \
    Pixel##dest_format *dest, int x, int width, \
    const Pixel##dest_format &color)

#define EXPORT_BITBLT(dest_format) EXPORT_SRC_PIXEL_FORMAT(EXPORT_DEST_BITBLT, EXPORT_IGNORE, dest_format)

EXPORT_DEST_PIXEL_FORMAT(EXPORT_BITBLT, EXPORT_IGNORE);
EXPORT_DEST_PIXEL_FORMAT(EXPORT_FILL, EXPORT_IGNORE);

}  // namespace display
}  // namespace esphome
