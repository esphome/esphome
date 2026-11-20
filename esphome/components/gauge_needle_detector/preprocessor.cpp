#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

namespace esphome::gauge_needle_detector {

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

namespace {

/// Largest free contiguous block the heap can hand out, or SIZE_MAX if unknown.
size_t largest_free_block() {
#ifdef USE_ESP32
  return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
#else
  return SIZE_MAX;
#endif
}

/// Grows a buffer, refusing the allocation without a large enough free block.
bool resize_buffer(std::vector<uint8_t> &buffer, size_t size, const char *name) {
  if (buffer.size() == size)
    return true;
  if (buffer.capacity() < size) {
    // A resize needs a new block before the old one is released.
    const size_t available = largest_free_block();
    if (available < size) {
      ESP_LOGE(TAG, "Not enough memory to resize %s: need %zu bytes, largest free block is %zu", name, size, available);
      return false;
    }
  }
  buffer.resize(size);
  return true;
}

void apply_clahe(uint8_t *img, int width, int height, std::array<uint16_t, 256> &hist, int tile_size = 8) {
  // Tiled histogram equalization, using the member histogram to avoid allocation.
  for (int tile_y = 0; tile_y < height; tile_y += tile_size) {
    for (int tile_x = 0; tile_x < width; tile_x += tile_size) {
      hist.fill(0);
      int tile_pixels = 0;

      for (int y = tile_y; y < std::min(tile_y + tile_size, height); y++) {
        for (int x = tile_x; x < std::min(tile_x + tile_size, width); x++) {
          hist[img[y * width + x]]++;
          tile_pixels++;
        }
      }

      // Cumulative distribution, in place.
      for (size_t i = 1; i < hist.size(); i++) {
        hist[i] += hist[i - 1];
      }

      for (int y = tile_y; y < std::min(tile_y + tile_size, height); y++) {
        for (int x = tile_x; x < std::min(tile_x + tile_size, width); x++) {
          const uint8_t val = img[y * width + x];
          if (tile_pixels > 0) {
            img[y * width + x] = static_cast<uint8_t>((hist[val] * 255) / tile_pixels);
          }
        }
      }
    }
  }
}

void erode(const uint8_t *src, uint8_t *dst, int width, int height, int kernel_size) {
  const int r = kernel_size / 2;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint8_t min_val = 255;
      for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
          const int ny = y + dy;
          const int nx = x + dx;
          if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
            const uint8_t val = src[ny * width + nx];
            if (val < min_val)
              min_val = val;
          }
        }
      }
      dst[y * width + x] = min_val;
    }
  }
}

void dilate_in_place(uint8_t *img, int width, int height, int kernel_size, uint8_t *tmp) {
  const int r = kernel_size / 2;
  memcpy(tmp, img, static_cast<size_t>(width) * height);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint8_t max_val = 0;
      for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
          const int ny = y + dy;
          const int nx = x + dx;
          if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
            const uint8_t val = tmp[ny * width + nx];
            if (val > max_val)
              max_val = val;
          }
        }
      }
      img[y * width + x] = max_val;
    }
  }
}

void apply_tophat(uint8_t *img, int width, int height, int radial_pixels, uint8_t *scratch, uint8_t *scratch2,
                  NeedleType needle_type, int min_kernel = 3, int radius_divisor = 10) {
  const size_t pixels = static_cast<size_t>(width) * height;
  int kernel_size = std::max(min_kernel, radial_pixels / radius_divisor);
  if (kernel_size % 2 == 0)
    kernel_size++;

  // White top hat: Original - Opening. Opening = Dilate(Erode(Original)).
  // Black top hat: Closing - Original. Closing = Erode(Dilate(Original)).
  if (needle_type == NEEDLE_TYPE_LIGHT) {
    erode(img, scratch, width, height, kernel_size);
    dilate_in_place(scratch, width, height, kernel_size, scratch2);
    for (size_t i = 0; i < pixels; i++) {
      const int val = static_cast<int>(img[i]) - static_cast<int>(scratch[i]);
      img[i] = (val < 0) ? 0 : static_cast<uint8_t>(val);
    }
  } else {
    memcpy(scratch, img, pixels);
    dilate_in_place(scratch, width, height, kernel_size, scratch2);
    erode(scratch, scratch2, width, height, kernel_size);
    for (size_t i = 0; i < pixels; i++) {
      const int val = static_cast<int>(scratch2[i]) - static_cast<int>(img[i]);
      img[i] = (val < 0) ? 0 : static_cast<uint8_t>(val);
    }
  }
}

void median_filter_3x3(uint8_t *img, int width, int height, uint8_t *scratch) {
  if (width < 3 || height < 3)
    return;

  memcpy(scratch, img, static_cast<size_t>(width) * height);

  // Fixed-size window on the stack, so no allocation per pixel.
  uint8_t window[9];
  for (int y = 1; y < height - 1; y++) {
    for (int x = 1; x < width - 1; x++) {
      int count = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          window[count++] = scratch[(y + dy) * width + (x + dx)];
        }
      }
      std::nth_element(window, window + 4, window + count);
      img[y * width + x] = window[4];
    }
  }
}

}  // namespace

void Preprocessor::reserve(int width, int height) {
  const size_t pixels = static_cast<size_t>(width) * height;
  this->working_buffer_.reserve(pixels);
  this->scratch_buffer_.reserve(pixels);
  this->scratch_buffer_2_.reserve(pixels);
}

bool Preprocessor::prepare(const uint8_t *raw, int width, int height, int radial_pixels, NeedleType needle_type,
                           bool debug) {
  const size_t required_size = static_cast<size_t>(width) * height;
  if (!resize_buffer(this->working_buffer_, required_size, "working"))
    return false;
  if (!resize_buffer(this->scratch_buffer_, required_size, "scratch1"))
    return false;
  if (!resize_buffer(this->scratch_buffer_2_, required_size, "scratch2"))
    return false;

  memcpy(this->working_buffer_.data(), raw, required_size);

  apply_clahe(this->working_buffer_.data(), width, height, this->histogram_);

  // Top-hat filter with a kernel that scales with the dial radius.
  apply_tophat(this->working_buffer_.data(), width, height, radial_pixels, this->scratch_buffer_.data(),
               this->scratch_buffer_2_.data(), needle_type);

  median_filter_3x3(this->working_buffer_.data(), width, height, this->scratch_buffer_2_.data());

  if (debug) {
    ESP_LOGD(TAG, "Preprocessing complete. NeedleType: %s, Radius: %d",
             needle_type == NEEDLE_TYPE_LIGHT ? LOG_STR_LITERAL("light") : LOG_STR_LITERAL("dark"), radial_pixels);
  }
  return true;
}

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
