#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SEVEN_SEGMENT_OCR

#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::seven_segment_ocr {

/// The digits read from one image of a seven-segment display.
struct OcrResult {
  uint32_t value{0};       // The digits as a number; only meaningful when complete().
  float confidence{0.0f};  // Mean segment decision margin, 0 (boundary) to 1 (decisive).
  uint8_t digits{0};       // Digit runs found, capped at the expected count.
  uint8_t expected{0};     // Digit count the caller asked for.
  uint8_t unknown{0};      // Runs whose lit segments matched no digit.
  bool overflow{false};    // The digits do not fit in value; they were not accumulated.

  /// True when exactly the expected digits were found, all of them were recognised
  /// and their value fits in 32 bits.
  bool complete() const {
    return this->digits == this->expected && this->unknown == 0 && !this->overflow && this->digits > 0;
  }
};

/// Reads the digits of a seven-segment display out of a grayscale image.
///
/// The image is thresholded in place, split into digit runs by a column
/// projection, and each run is sampled at the seven segment positions. A run
/// whose lit segments match a digit contributes that digit to the result.
/// Digits must be bright on a dark background. A run narrower than a third
/// of the image height is read as a 1 when both of its halves are lit.
class SevenSegmentOCR final {
 public:
  /// Allocates the column projection. Call once with the widest image width.
  /// Returns false when the allocation fails, in which case recognize() returns an empty result.
  bool reserve(int width);

  /// Thresholds image in place and reads up to expected_digits digits.
  /// The image must be at most as wide as the width passed to reserve().
  OcrResult recognize(uint8_t *image, int width, int height, uint8_t expected_digits);

  /// Pixels above this level count as lit. Defaults to 128.
  void set_threshold_level(uint8_t level) { this->threshold_level_ = level; }
  uint8_t get_threshold_level() const { return this->threshold_level_; }

 protected:
  static int8_t recognize_digit(const uint8_t *image, int width, int height, int stride, float *confidence);
  static int8_t recognize_narrow(const uint8_t *image, int width, int height, int stride, float *confidence);

  RAMUniquePtr<uint16_t[]> column_sums_;
  size_t column_capacity_{0};
  uint8_t threshold_level_{128};
};

}  // namespace esphome::seven_segment_ocr

#endif  // USE_SEVEN_SEGMENT_OCR
