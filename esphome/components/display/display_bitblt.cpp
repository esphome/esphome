#include "display.h"

#include "pixel_helpers.h"
#include "pixel_bitblt.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAG = "display";

template<typename PixelFormat, typename Fn>
static bool display_direct_draw(int x, int y, int width, int height,
  const PixelFormat *src, int src_x, int src_stride,
  Fn draw_pixels) {

  // If pixel is offset, it requires buffer re-alignement, or direct rendering
  if (PixelFormat::PACKED_PIXELS > 1) {
    auto packed_pixel = PixelFormat::packed_pixel(x);
    if (packed_pixel != src_x)
      return false;
  }

  auto start_us = micros();

  bool done = draw_pixels(
    x, y, width, height,
    (const uint8_t*)src, src_stride, src_stride
  );

  if (done) {
    ESP_LOGV(TAG, "Direct Draw: %dx%d/%dx%d, src_x=%d, stride=%d, format=%d, time_us=%d",
      x, y, width, height,
      src_x, src_stride, PixelFormat::FORMAT,
      micros() - start_us);
  }

  return done;
}

template<typename DestPixelFormat, typename SrcPixelFormat, typename Fn>
static bool display_convert_draw(
  int x, int y, int width, int height,
  const SrcPixelFormat *src, int src_x, int src_stride,
  Color color_on, Color color_off, Fn draw_pixels
) {
  auto width_stride = DestPixelFormat::array_stride(DestPixelFormat::packed_pixel(x) + width);
  auto width_bytes_stride = DestPixelFormat::bytes_stride(DestPixelFormat::packed_pixel(x) + width);
  auto dest = new DestPixelFormat[width_stride];
  bool done = true;
  DestPixelFormat dest_on, dest_off;

  if (SrcPixelFormat::COLOR_KEY) {
    dest_on = from_color<DestPixelFormat>(color_on);
    dest_off = from_color<DestPixelFormat>(color_off);
  }

  auto start_us = micros();

  for (int j = 0; j < height && done; j++) {
    bitblt<SrcPixelFormat, DestPixelFormat, false>(
      dest, DestPixelFormat::packed_pixel(x),
      src, src_x, width,
      dest_on, dest_off
    );
    src = (const SrcPixelFormat*)((const uint8_t*)src + src_stride);

    done = draw_pixels(
      x, y + j, width, 1,
      (const uint8_t*)dest, width_bytes_stride, width_bytes_stride
    );
  }

  if (done) {
    ESP_LOGV(TAG, "%s Draw: %dx%d/%dx%d, src_x=%d, stride=%d, src_format=%d, dest_format=%d, time_us=%d",
      SrcPixelFormat::COLOR_KEY ? "Keyed" : "Convert",
      x, y, width, height,
      src_x, src_stride,
      SrcPixelFormat::FORMAT, DestPixelFormat::FORMAT,
      micros() - start_us);
  }

  delete [] dest;
  return done;
}

template<typename DestPixelFormat, typename SrcPixelFormat, typename Fn>
static bool display_convert_buffer(
  int x, int y, int width, int height,
  const SrcPixelFormat *src, int src_x, int src_stride,
  Color color_on, Color color_off, Fn get_pixels
) {
  DestPixelFormat dest_on, dest_off;

  if (SrcPixelFormat::COLOR_KEY) {
    dest_on = from_color<DestPixelFormat>(color_on);
    dest_off = from_color<DestPixelFormat>(color_off);
  }

  auto start_us = micros();

  for (int j = 0; j < height; j++) {
    auto dest = (DestPixelFormat*)get_pixels(y + j);
    if (!dest)
      return false;

    bitblt<SrcPixelFormat, DestPixelFormat, true>(
      dest, x,
      src, src_x, width,
      dest_on, dest_off);
    src = (const SrcPixelFormat*)((const uint8_t*)src + src_stride);
  }

  ESP_LOGV(TAG, "%s Blt: %dx%d/%dx%d, src_x=%d, stride=%d, src_format=%d, dest_format=%d, time_us=%d",
    SrcPixelFormat::COLOR_KEY ? "Keyed" : "Convert",
    x, y, width, height,
    src_x, src_stride,
    SrcPixelFormat::FORMAT, DestPixelFormat::FORMAT,
    micros() - start_us);

  return true;
}

bool HOT Display::draw_pixels_at(int x, int y, int width, int height, const uint8_t *data, int stride, PixelFormat data_format, Color color_on, Color color_off) {
  ESP_LOGV(TAG, "DrawFormat: %dx%d/%dx%d, size=%d, format=%d=>%d",
    x, y, width, height,
    stride * height, data_format, this->get_native_pixel_format());

  int min_x, max_x, min_y, max_y;
  if (!this->clamp_x_(x, width, min_x, max_x))
    return true;
  if (!this->clamp_y_(y, height, min_y, max_y))
    return true;

  auto get_pixels = [this](int y) {
    return this->get_native_pixels_(y);
  };
  auto draw_pixels = [this](int x, int y, int w, int h,
    const uint8_t *data, int data_line_size, int data_stride) {
    return this->draw_pixels_(x, y, w, h, data, data_line_size, data_stride);
  };

  #define CONVERT_IGNORE_FORMAT(format) \
    case PixelFormat::format: \
      break

  #define CONVERT_DEST_FORMAT(dest_format) \
    case PixelFormat::dest_format: \
      if (display_convert_buffer<Pixel##dest_format>( \
        min_x, min_y, max_x - min_x, max_y - min_y, \
        src_data, src_pixel_offset, stride, \
        color_on, color_off, get_pixels)) \
        return true; \
      if (display_convert_draw<Pixel##dest_format>( \
        min_x, min_y, max_x - min_x, max_y - min_y, \
        src_data, src_pixel_offset, stride, \
        color_on, color_off, draw_pixels)) \
        return true; \
      break

  #define CONVERT_SRC_FORMAT(src_format) \
    case PixelFormat::src_format: \
      { \
        auto src_data = offset_buffer((const Pixel##src_format*)data, min_x - x, min_y - y, width); \
        auto src_pixel_offset = Pixel##src_format::packed_pixel(min_x - x); \
        auto native_format = this->get_native_pixel_format(); \
        if (native_format == PixelFormat::src_format) { \
          if (display_direct_draw( \
            min_x, min_y, max_x - min_x, max_y - min_y, \
            src_data, src_pixel_offset, stride, \
            draw_pixels)) \
            return true; \
        } \
        switch (native_format) { \
          EXPORT_DEST_PIXEL_FORMAT(CONVERT_DEST_FORMAT, CONVERT_IGNORE_FORMAT); \
        } \
      } \
      break

  switch (data_format) {
    EXPORT_SRC_PIXEL_FORMAT(CONVERT_SRC_FORMAT, CONVERT_IGNORE_FORMAT);
  }

  return false;
}

template<typename Format, typename Fn>
static bool display_filled_rectangle_alloc(int x, int y, int width, int height, Color color, Fn draw_pixels) {
  auto width_stride = Format::array_stride(width + Format::packed_pixel(x));
  auto dest = new Format[width_stride];
  auto pixel_color = from_color<Format>(color);

  fill(dest, x, width_stride * Format::PACKED_PIXELS, pixel_color);

  bool done = draw_pixels(
    x, y, width, height,
    (const uint8_t*)dest, width_stride, 0
  );

  delete [] dest;
  return done;
}

template<typename Format, typename Fn>
static bool display_filled_rectangle_buffer(int x, int y, int width, int height, Color color, Fn get_pixels) {
  auto pixel_color = from_color<Format>(color);

  for (int j = 0; j < height; j++) {
    auto dest = (Format*)get_pixels(y + j);
    if (!dest)
      return false;

    fill(dest, x, width, pixel_color);
  }

  return true;
}

bool Display::filled_rectangle_(int x, int y, int width, int height, Color color) {
  int min_x, max_x, min_y, max_y;

  if (!this->clamp_x_(x, width, min_x, max_x))
    return true;
  if (!this->clamp_y_(y, height, min_y, max_y))
    return true;

  auto get_pixels = [this](int y) {
    return this->get_native_pixels_(y);
  };
  auto draw_pixels = [this](int x, int y, int w, int h,
    const uint8_t *data, int data_line_size, int data_stride) {
    return this->draw_pixels_(x, y, w, h, data, data_line_size, data_stride);
  };

  #define FILLED_RECT_FORMAT(format) \
    case PixelFormat::format: \
      if (display_filled_rectangle_buffer<Pixel##format>( \
          min_x, min_y, max_x - min_x, max_y - min_y, \
          color, get_pixels)) \
        return true; \
      if (display_filled_rectangle_alloc<Pixel##format>( \
          min_x, min_y, max_x - min_x, max_y - min_y, \
          color, draw_pixels)) \
        return true; \
      break

  #define FILLED_RECT_IGNORE(format) \
    case PixelFormat::format: \
      break

  switch (this->get_native_pixel_format()) {
    EXPORT_DEST_PIXEL_FORMAT(FILLED_RECT_FORMAT, FILLED_RECT_IGNORE);
  }

  return false;
}

} // display
} // esphome
