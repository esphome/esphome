#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::gauge_needle_detector {

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

namespace {

/// Stretches the histogram, clipping dominant bins so a large flat background
/// cannot flatten the mapping of the needle. Unlike per-tile equalization this
/// keeps one mapping for the whole crop, so it adds no tile edges.
void apply_contrast_stretch(uint8_t *img, size_t pixels, std::array<uint32_t, 256> &hist) {
  hist.fill(0);
  for (size_t i = 0; i < pixels; i++)
    hist[img[i]]++;

  const uint32_t limit = std::max<uint32_t>(1, static_cast<uint32_t>(pixels / 32));
  uint32_t excess = 0;
  for (auto &bin : hist) {
    if (bin > limit) {
      excess += bin - limit;
      bin = limit;
    }
  }
  const uint32_t add = excess / 256;
  for (auto &bin : hist)
    bin += add;

  for (size_t i = 1; i < hist.size(); i++)
    hist[i] += hist[i - 1];

  const uint32_t total = hist[255];
  if (total == 0)
    return;
  for (size_t i = 0; i < pixels; i++)
    img[i] = static_cast<uint8_t>((hist[img[i]] * 255u) / total);
}

enum class MorphOp : uint8_t { MORPH_OP_ERODE, MORPH_OP_DILATE };

inline uint8_t morph_pick(uint8_t a, uint8_t b, MorphOp op) {
  return op == MorphOp::MORPH_OP_ERODE ? std::min(a, b) : std::max(a, b);
}

/// One horizontal pass of a square min/max kernel.
void morph_horizontal(const uint8_t *src, uint8_t *dst, int width, int height, int r, MorphOp op) {
  for (int y = 0; y < height; y++) {
    const uint8_t *row = src + static_cast<size_t>(y) * width;
    uint8_t *out = dst + static_cast<size_t>(y) * width;
    for (int x = 0; x < width; x++) {
      const int x0 = std::max(0, x - r);
      const int x1 = std::min(width - 1, x + r);
      uint8_t best = row[x0];
      for (int nx = x0 + 1; nx <= x1; nx++)
        best = morph_pick(best, row[nx], op);
      out[x] = best;
    }
  }
}

/// One vertical pass of a square min/max kernel.
void morph_vertical(const uint8_t *src, uint8_t *dst, int width, int height, int r, MorphOp op) {
  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      const int y0 = std::max(0, y - r);
      const int y1 = std::min(height - 1, y + r);
      uint8_t best = src[static_cast<size_t>(y0) * width + x];
      for (int ny = y0 + 1; ny <= y1; ny++)
        best = morph_pick(best, src[static_cast<size_t>(ny) * width + x], op);
      dst[static_cast<size_t>(y) * width + x] = best;
    }
  }
}

/// A square min/max kernel is separable, so two 1-D passes replace the k*k scan.
/// `horizontal` must differ from `src`; `result` may be `src` because the
/// horizontal pass finishes before the vertical pass writes it.
void morph(const uint8_t *src, uint8_t *horizontal, uint8_t *result, int width, int height, int r, MorphOp op) {
  morph_horizontal(src, horizontal, width, height, r, op);
  morph_vertical(horizontal, result, width, height, r, op);
}

void apply_tophat(uint8_t *img, int width, int height, int radial_pixels, uint8_t *scratch, uint8_t *scratch_2,
                  NeedleType needle_type, int min_kernel = 3, int radius_divisor = 10) {
  int kernel_size = std::max(min_kernel, radial_pixels / radius_divisor);
  if (kernel_size % 2 == 0)
    kernel_size++;
  const int r = kernel_size / 2;
  const size_t pixels = static_cast<size_t>(width) * height;

  // White top hat: Original - Opening. Opening = Dilate(Erode(Original)).
  // Black top hat: Closing - Original. Closing = Erode(Dilate(Original)).
  if (needle_type == NEEDLE_TYPE_LIGHT) {
    morph(img, scratch, scratch_2, width, height, r, MorphOp::MORPH_OP_ERODE);
    morph(scratch_2, scratch, scratch_2, width, height, r, MorphOp::MORPH_OP_DILATE);
    for (size_t i = 0; i < pixels; i++) {
      const int val = static_cast<int>(img[i]) - static_cast<int>(scratch_2[i]);
      img[i] = (val < 0) ? 0 : static_cast<uint8_t>(val);
    }
  } else {
    morph(img, scratch, scratch_2, width, height, r, MorphOp::MORPH_OP_DILATE);
    morph(scratch_2, scratch, scratch_2, width, height, r, MorphOp::MORPH_OP_ERODE);
    for (size_t i = 0; i < pixels; i++) {
      const int val = static_cast<int>(scratch_2[i]) - static_cast<int>(img[i]);
      img[i] = (val < 0) ? 0 : static_cast<uint8_t>(val);
    }
  }
}

}  // namespace

Preprocessor::~Preprocessor() { this->release_(); }

void Preprocessor::release_() {
  if (this->working_ != nullptr)
    this->allocator_.deallocate(this->working_, this->capacity_);
  if (this->scratch_ != nullptr)
    this->allocator_.deallocate(this->scratch_, this->capacity_);
  if (this->scratch_2_ != nullptr)
    this->allocator_.deallocate(this->scratch_2_, this->capacity_);
  this->working_ = nullptr;
  this->scratch_ = nullptr;
  this->scratch_2_ = nullptr;
  this->capacity_ = 0;
}

bool Preprocessor::reserve(int width, int height) {
  if (width <= 0 || height <= 0)
    return false;
  const size_t pixels = static_cast<size_t>(width) * height;
  if (pixels <= this->capacity_)
    return this->working_ != nullptr;

  this->release_();
  this->working_ = this->allocator_.allocate(pixels);
  this->scratch_ = this->allocator_.allocate(pixels);
  this->scratch_2_ = this->allocator_.allocate(pixels);
  if (this->working_ == nullptr || this->scratch_ == nullptr || this->scratch_2_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate three %zu-byte detector buffers", pixels);
    this->release_();
    return false;
  }
  this->capacity_ = pixels;
  return true;
}

bool Preprocessor::prepare(const uint8_t *raw, int width, int height, int radial_pixels, NeedleType needle_type,
                           bool debug) {
  if (raw == nullptr || width <= 0 || height <= 0)
    return false;
  const size_t required_size = static_cast<size_t>(width) * height;
  if (this->working_ == nullptr || required_size > this->capacity_) {
    ESP_LOGE(TAG, "Crop %dx%d needs %zu bytes but only %zu are reserved", width, height, required_size,
             this->capacity_);
    return false;
  }

  memcpy(this->working_, raw, required_size);
  apply_contrast_stretch(this->working_, required_size, this->histogram_);
  apply_tophat(this->working_, width, height, radial_pixels, this->scratch_, this->scratch_2_, needle_type);

  if (debug) {
    ESP_LOGD(TAG, "Preprocessing complete. NeedleType: %s, Radius: %d",
             needle_type == NEEDLE_TYPE_LIGHT ? LOG_STR_LITERAL("light") : LOG_STR_LITERAL("dark"), radial_pixels);
  }
  return true;
}

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
