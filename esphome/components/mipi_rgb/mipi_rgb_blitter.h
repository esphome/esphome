#pragma once

#include "esphome/components/display/display.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace esphome::mipi_rgb {

inline void rotate_point(int &x, int &y, int width, int height, display::DisplayRotation rotation) {
  switch (rotation) {
    case display::DISPLAY_ROTATION_0_DEGREES:
      break;
    case display::DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = width - x - 1;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      x = width - x - 1;
      y = height - y - 1;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = height - y - 1;
      break;
  }
}

inline void blit_rotated_565(uint16_t *fb, int fb_width, int fb_height, display::DisplayRotation rotation,
                             const uint16_t *src_origin, int x_start, int y_start, int w, int h, size_t stride) {
  switch (rotation) {
    case display::DISPLAY_ROTATION_0_DEGREES: {
      for (int py = 0; py < h; py++) {
        memcpy(&fb[(y_start + py) * fb_width + x_start], src_origin + static_cast<size_t>(py) * stride,
               w * sizeof(uint16_t));
      }
      break;
    }
    case display::DISPLAY_ROTATION_90_DEGREES: {
      const size_t x_dest_offset = fb_width - y_start - h;
      for (int px = 0; px < w; px++) {
        uint16_t *dest = &fb[(x_start + px) * fb_width + x_dest_offset];
        for (int py = 0; py < h; py++) {
          *dest++ = src_origin[static_cast<size_t>(h - 1 - py) * stride + px];
        }
      }
      break;
    }
    case display::DISPLAY_ROTATION_180_DEGREES: {
      const size_t x_dest_offset = fb_width - x_start - w;
      for (int py = 0; py < h; py++) {
        uint16_t *dest = &fb[(fb_height - 1 - (y_start + py)) * fb_width + x_dest_offset];
        const uint16_t *src_row = src_origin + static_cast<size_t>(py) * stride;
        for (int px = 0; px < w; px++) {
          *dest++ = src_row[w - 1 - px];
        }
      }
      break;
    }
    case display::DISPLAY_ROTATION_270_DEGREES: {
      for (int px = 0; px < w; px++) {
        uint16_t *dest = &fb[(fb_height - 1 - x_start - px) * fb_width + y_start];
        const uint16_t *s = src_origin + px;
        for (int py = 0; py < h; py++) {
          *dest++ = *s;
          s += stride;
        }
      }
      break;
    }
  }
}

}  // namespace esphome::mipi_rgb
