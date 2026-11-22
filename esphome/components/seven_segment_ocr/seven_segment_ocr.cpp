#include "seven_segment_ocr.h"

#ifdef USE_SEVEN_SEGMENT_OCR

#include "esphome/core/hal.h"

#include <algorithm>
#include <cmath>

namespace esphome::seven_segment_ocr {

namespace {

// Segment order: A top, B top right, C bottom right, D bottom, E bottom left, F top left, G middle.
// Only the vertical probe position is fixed; the horizontal one is read from the stroke columns.
const uint8_t PROBE_Y[7] PROGMEM = {38, 77, 179, 217, 179, 77, 128};

// Segment mask of each digit; bit i is set when segment i is lit.
const uint8_t DIGIT_MASKS[10] PROGMEM = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};

/// Fraction of the 3x3 window centred on (x, y) that is lit.
float probe_ratio(const uint8_t *image, int width, int height, int stride, int x, int y) {
  int lit = 0;
  int sampled = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      const int probe_x = x + dx;
      const int probe_y = y + dy;
      if (probe_x < 0 || probe_x >= width || probe_y < 0 || probe_y >= height)
        continue;
      sampled++;
      if (image[probe_y * stride + probe_x] == 255)
        lit++;
    }
  }
  return sampled > 0 ? static_cast<float>(lit) / static_cast<float>(sampled) : 0.0f;
}

}  // namespace

bool SevenSegmentOCR::reserve(int width) {
  if (width <= 0)
    return false;
  this->column_sums_ = RAMAllocator<uint16_t>().make_unique_array_for_overwrite(static_cast<size_t>(width));
  this->column_capacity_ = this->column_sums_ != nullptr ? static_cast<size_t>(width) : 0;
  return this->column_sums_ != nullptr;
}

OcrResult SevenSegmentOCR::recognize(uint8_t *image, int width, int height, uint8_t expected_digits) {
  OcrResult result;
  result.expected = expected_digits;
  if (image == nullptr || width <= 0 || height <= 0 || expected_digits == 0)
    return result;
  if (static_cast<size_t>(width) > this->column_capacity_)
    return result;

  // Walk the rows so the pixels are read sequentially, and count the lit pixels of every column.
  std::fill_n(this->column_sums_.get(), width, 0);
  for (int y = 0; y < height; y++) {
    uint8_t *row = image + static_cast<size_t>(y) * width;
    for (int x = 0; x < width; x++) {
      row[x] = row[x] > this->threshold_level_ ? 255 : 0;
      if (row[x] == 255)
        this->column_sums_[x]++;
    }
  }

  const uint16_t min_signal = std::max<uint16_t>(1, static_cast<uint16_t>(height / 40));
  const uint16_t min_peak = static_cast<uint16_t>(height / 3);
  const int min_width = std::max(2, height / 30);
  float confidence_sum = 0.0f;
  uint8_t recognised = 0;
  int run_start = -1;
  for (int x = 0; x <= width; x++) {
    const bool signal = (x < width) && (this->column_sums_[x] > min_signal);
    if (signal && run_start < 0) {
      run_start = x;
    } else if (!signal && run_start >= 0) {
      const int digit_x = run_start;
      const int digit_width = x - digit_x;
      run_start = -1;
      if (digit_width < min_width)
        continue;

      // A decimal point or a short mark can pass the column threshold, but no digit is this short.
      uint16_t peak = 0;
      for (int column = digit_x; column < x; column++)
        peak = std::max(peak, this->column_sums_[column]);
      if (peak < min_peak)
        continue;

      // Count every run so complete() can tell an aligned crop from one with extra digits.
      if (result.digits < UINT8_MAX)
        result.digits++;
      if (result.digits > expected_digits)
        continue;

      float confidence = 0.0f;
      const bool narrow = digit_width * 3 < height;
      const int8_t digit = narrow ? recognize_narrow(image + digit_x, digit_width, height, width, &confidence)
                                  : recognize_digit(image + digit_x, this->column_sums_.get() + digit_x, digit_width,
                                                    height, width, &confidence);
      if (digit < 0) {
        result.unknown++;
      } else if (result.value > (UINT32_MAX - static_cast<uint32_t>(digit)) / 10u) {
        result.overflow = true;
      } else {
        result.value = result.value * 10 + static_cast<uint32_t>(digit);
        confidence_sum += confidence;
        recognised++;
      }
    }
  }

  if (recognised > 0)
    result.confidence = confidence_sum / recognised;
  return result;
}

int8_t SevenSegmentOCR::recognize_narrow(const uint8_t *image, int width, int height, int stride, float *confidence) {
  const int probe_x = width / 2;
  float margin = 0.0f;
  bool complete = true;
  for (uint8_t half = 0; half < 2; half++) {
    const int probe_y = half == 0 ? height / 3 : (2 * height) / 3;
    const float ratio = probe_ratio(image, width, height, stride, probe_x, probe_y);
    margin += std::abs(ratio - 0.5f) * 2.0f;
    if (ratio <= 0.5f)
      complete = false;
  }
  if (confidence != nullptr)
    *confidence = margin / 2.0f;
  return complete ? 1 : -1;
}

int8_t SevenSegmentOCR::recognize_digit(const uint8_t *image, const uint16_t *columns, int width, int height,
                                        int stride, float *confidence) {
  // The horizontal strokes span the whole run, so the run width does not say where
  // the vertical strokes are. Use the strongest column of each half for them, which
  // also covers displays whose strokes sit at the outer edges.
  int left_best = -1;
  int right_best = -1;
  int left_sum = 0;
  int right_sum = 0;
  int left_columns = 0;
  int right_columns = 0;
  for (int x = 0; x < width; x++) {
    if (x < width / 2) {
      if (columns[x] > left_best) {
        left_best = columns[x];
        left_sum = x;
        left_columns = 1;
      } else if (columns[x] == left_best) {
        left_sum += x;
        left_columns++;
      }
    } else if (columns[x] > right_best) {
      right_best = columns[x];
      right_sum = x;
      right_columns = 1;
    } else if (columns[x] == right_best) {
      right_sum += x;
      right_columns++;
    }
  }
  const int left_x = left_columns > 0 ? left_sum / left_columns : 0;
  const int right_x = right_columns > 0 ? right_sum / right_columns : width - 1;

  uint8_t mask = 0;
  float margin = 0.0f;
  for (uint8_t i = 0; i < 7; i++) {
    const int probe_y = static_cast<int>(progmem_read_byte(&PROBE_Y[i])) * height / 255;
    const int probe_x = (i == 1 || i == 2) ? right_x : ((i == 4 || i == 5) ? left_x : width / 2);
    const float ratio = probe_ratio(image, width, height, stride, probe_x, probe_y);
    margin += std::abs(ratio - 0.5f) * 2.0f;
    if (ratio > 0.5f)
      mask |= static_cast<uint8_t>(1 << i);
  }
  if (confidence != nullptr)
    *confidence = margin / 7.0f;
  for (uint8_t i = 0; i < 10; i++) {
    if (progmem_read_byte(&DIGIT_MASKS[i]) == mask)
      return static_cast<int8_t>(i);
  }
  return -1;
}

}  // namespace esphome::seven_segment_ocr

#endif  // USE_SEVEN_SEGMENT_OCR
