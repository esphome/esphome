#pragma once

// required for clang-tidy
#ifndef LV_CONF_H
#define LV_CONF_SKIP 1  // NOLINT
#endif                  // LV_CONF_H

#include <lvgl.h>

#include <algorithm>

namespace esphome::lvgl {

// Edge of the square tiles used for software rotation by 90 and 270 degrees.
// 32 RGB565 pixels are 64 bytes, one cache line of the ESP32-S3 PSRAM cache.
static constexpr lv_coord_t ROTATE_TILE_SIZE = 32;

/**
 * Rotate a width x height block of pixels clockwise by 90 degrees.
 *
 * The block is copied in tiles so each destination line gets a run of pixels instead of one scattered pixel,
 * which is much faster when the destination is in PSRAM.
 *
 * @param src Source pixels, row by row, width pixels per row.
 * @param dst Destination, width lines of dst_stride pixels; the first height pixels of each line are written.
 */
template<typename T>
void rotate_90_tiled(const T *src, T *dst, lv_coord_t width, lv_coord_t height, lv_coord_t dst_stride) {
  for (lv_coord_t row_start = 0; row_start < height; row_start += ROTATE_TILE_SIZE) {
    const lv_coord_t row_end = std::min<lv_coord_t>(row_start + ROTATE_TILE_SIZE, height);
    for (lv_coord_t col_start = 0; col_start < width; col_start += ROTATE_TILE_SIZE) {
      const lv_coord_t col_end = std::min<lv_coord_t>(col_start + ROTATE_TILE_SIZE, width);
      for (lv_coord_t col = col_start; col != col_end; col++) {
        // The source moves by an index and the destination is decremented before the write, so no pointer
        // ever leaves its buffer.
        const T *in = src + col;
        T *out = dst + col * dst_stride + (height - row_start);
        for (lv_coord_t offset = row_start * width, end = row_end * width; offset != end; offset += width) {
          *--out = in[offset];
        }
      }
    }
  }
}

/**
 * Rotate a width x height block of pixels clockwise by 270 degrees, tiled like rotate_90_tiled().
 */
template<typename T>
void rotate_270_tiled(const T *src, T *dst, lv_coord_t width, lv_coord_t height, lv_coord_t dst_stride) {
  for (lv_coord_t row_start = 0; row_start < height; row_start += ROTATE_TILE_SIZE) {
    const lv_coord_t row_end = std::min<lv_coord_t>(row_start + ROTATE_TILE_SIZE, height);
    for (lv_coord_t col_start = 0; col_start < width; col_start += ROTATE_TILE_SIZE) {
      const lv_coord_t col_end = std::min<lv_coord_t>(col_start + ROTATE_TILE_SIZE, width);
      for (lv_coord_t col = col_start; col != col_end; col++) {
        const T *in = src + col;
        T *out = dst + (width - 1 - col) * dst_stride + row_start;
        for (lv_coord_t offset = row_start * width, end = row_end * width; offset != end; offset += width) {
          *out++ = in[offset];
        }
      }
    }
  }
}

}  // namespace esphome::lvgl
