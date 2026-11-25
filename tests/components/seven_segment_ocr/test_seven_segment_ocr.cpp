#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "esphome/components/seven_segment_ocr/seven_segment_ocr.h"

namespace esphome::seven_segment_ocr {
namespace {

constexpr int DIGIT_GAP = 10;

struct Metrics {
  int width;
  int height;
  int stroke;
  int left;
  int right;
  int top;
  int middle;
  int bottom;
};

constexpr Metrics NORMAL{60, 90, 7, 3, 56, 13, 45, 76};
constexpr Metrics SMALL{20, 30, 3, 4, 15, 4, 15, 25};

// Segment mask of each digit, in the engine's segment order.
constexpr uint8_t DIGIT_MASKS[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};

struct Image {
  std::vector<uint8_t> pixels;
  int width;
};

void fill_rect(uint8_t *image, int width, const Metrics &metrics, int x0, int y0, int x1, int y1) {
  for (int y = std::max(0, y0); y <= std::min(metrics.height - 1, y1); y++) {
    for (int x = std::max(0, x0); x <= std::min(width - 1, x1); x++)
      image[y * width + x] = 255;
  }
}

void draw_segment(uint8_t *image, int width, int x0, int segment, const Metrics &metrics) {
  const int half = metrics.stroke / 2;
  switch (segment) {
    case 0:  // A: top
      fill_rect(image, width, metrics, x0, metrics.top - half, x0 + metrics.width - 1, metrics.top + half);
      break;
    case 1:  // B: top right
      fill_rect(image, width, metrics, x0 + metrics.right - half, metrics.top + 2, x0 + metrics.right + half,
                metrics.middle - 2);
      break;
    case 2:  // C: bottom right
      fill_rect(image, width, metrics, x0 + metrics.right - half, metrics.middle + 2, x0 + metrics.right + half,
                metrics.bottom - 2);
      break;
    case 3:  // D: bottom
      fill_rect(image, width, metrics, x0, metrics.bottom - half, x0 + metrics.width - 1, metrics.bottom + half);
      break;
    case 4:  // E: bottom left
      fill_rect(image, width, metrics, x0 + metrics.left - half, metrics.middle + 2, x0 + metrics.left + half,
                metrics.bottom - 2);
      break;
    case 5:  // F: top left
      fill_rect(image, width, metrics, x0 + metrics.left - half, metrics.top + 2, x0 + metrics.left + half,
                metrics.middle - 2);
      break;
    case 6:  // G: middle
      fill_rect(image, width, metrics, x0, metrics.middle - half, x0 + metrics.width - 1, metrics.middle + half);
      break;
    default:
      break;
  }
}

Image render_mask(uint8_t mask, const Metrics &metrics = NORMAL) {
  Image image{std::vector<uint8_t>(static_cast<size_t>(metrics.width) * metrics.height, 0), metrics.width};
  for (int segment = 0; segment < 7; segment++) {
    if ((mask & (1 << segment)) != 0)
      draw_segment(image.pixels.data(), image.width, 0, segment, metrics);
  }
  return image;
}

Image render(const std::string &digits, const Metrics &metrics = NORMAL) {
  const int count = static_cast<int>(digits.size());
  const int width = count * metrics.width + (count - 1) * DIGIT_GAP;
  Image image{std::vector<uint8_t>(static_cast<size_t>(width) * metrics.height, 0), width};
  for (int i = 0; i < count; i++) {
    const uint8_t mask = DIGIT_MASKS[digits[static_cast<size_t>(i)] - '0'];
    for (int segment = 0; segment < 7; segment++) {
      if ((mask & (1 << segment)) != 0)
        draw_segment(image.pixels.data(), image.width, i * (metrics.width + DIGIT_GAP), segment, metrics);
    }
  }
  return image;
}

void draw_point(Image &image, int x, int y, const Metrics &metrics) {
  fill_rect(image.pixels.data(), image.width, metrics, x, y, x + 5, y + 5);
}

}  // namespace

TEST(SevenSegmentOcr, ReadsEveryDigit) {
  for (char digit = '0'; digit <= '9'; digit++) {
    Image image = render(std::string(1, digit));
    SevenSegmentOCR ocr;
    ASSERT_TRUE(ocr.reserve(image.width));
    const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1);
    EXPECT_TRUE(result.complete()) << "digit " << digit;
    EXPECT_EQ(result.value, static_cast<uint32_t>(digit - '0')) << "digit " << digit;
    EXPECT_EQ(result.unknown, 0);
    EXPECT_NEAR(result.confidence, 1.0f, 0.01f);
  }
}

TEST(SevenSegmentOcr, ReadsEveryDigitAtSmallScale) {
  for (char digit = '0'; digit <= '9'; digit++) {
    Image image = render(std::string(1, digit), SMALL);
    SevenSegmentOCR ocr;
    ASSERT_TRUE(ocr.reserve(image.width));
    const OcrResult result = ocr.recognize(image.pixels.data(), image.width, SMALL.height, 1);
    EXPECT_TRUE(result.complete()) << "digit " << digit;
    EXPECT_EQ(result.value, static_cast<uint32_t>(digit - '0')) << "digit " << digit;
  }
}

TEST(SevenSegmentOcr, ReadsSeveralDigits) {
  Image image = render("120875");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 6);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.digits, 6);
  EXPECT_EQ(result.value, 120875u);
}

TEST(SevenSegmentOcr, ReadsDigitsWithInsetStrokes) {
  Metrics inset = NORMAL;
  inset.left = 12;
  inset.right = 48;
  Image image = render("120875", inset);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 6);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.value, 120875u);
  EXPECT_NEAR(result.confidence, 1.0f, 0.01f);
}

TEST(SevenSegmentOcr, ReadsThinStrokedSeven) {
  Metrics thin = NORMAL;
  thin.stroke = 3;
  Image image = render("7", thin);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.value, 7u);
}

TEST(SevenSegmentOcr, SkipsDecimalPoint) {
  Image image = render("125");
  draw_point(image, NORMAL.width + (DIGIT_GAP - 6) / 2, NORMAL.bottom - 4, NORMAL);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 3);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.digits, 3);
  EXPECT_EQ(result.value, 125u);
}

TEST(SevenSegmentOcr, RejectsExtraDigits) {
  Image image = render("12345678");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 4);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 8);
  EXPECT_EQ(result.value, 1234u);
}

TEST(SevenSegmentOcr, ReportsMissingDigits) {
  Image image = render("12");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 6);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 2);
  EXPECT_EQ(result.expected, 6);
  EXPECT_EQ(result.value, 12u);
}

TEST(SevenSegmentOcr, ReportsUnknownSegments) {
  // Top and bottom strokes with the right side closed: one tall run, but no digit mask.
  Image image = render_mask((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 1);
  EXPECT_EQ(result.unknown, 1);
  EXPECT_EQ(result.confidence, 0.0f);
}

TEST(SevenSegmentOcr, ReportsHalfHeightBarAsUnknown) {
  Image image{std::vector<uint8_t>(static_cast<size_t>(SMALL.width) * SMALL.height, 0), SMALL.width};
  fill_rect(image.pixels.data(), image.width, SMALL, 8, 2, 10, 14);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, SMALL.height, 1);
  EXPECT_EQ(result.digits, 1);
  EXPECT_EQ(result.unknown, 1);
  EXPECT_FALSE(result.complete());
}

TEST(SevenSegmentOcr, RejectsOverflowingValue) {
  Image image = render("9999999999");  // Ten nines do not fit in a uint32_t.
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 10);
  EXPECT_FALSE(result.complete());
  EXPECT_TRUE(result.overflow);
  EXPECT_EQ(result.digits, 10);
}

TEST(SevenSegmentOcr, ReadsLargestValue) {
  Image image = render("4294967295");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 10);
  EXPECT_TRUE(result.complete());
  EXPECT_FALSE(result.overflow);
  EXPECT_EQ(result.value, UINT32_MAX);
}

TEST(SevenSegmentOcr, RejectsUnreservedWidth) {
  Image image = render("1");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width - 1));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 0);
}

TEST(SevenSegmentOcr, RejectsEmptyInput) {
  Image image = render("1");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  EXPECT_FALSE(ocr.recognize(nullptr, image.width, NORMAL.height, 1).complete());
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), 0, NORMAL.height, 1).complete());
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), image.width, 0, 1).complete());
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 0).complete());
}

TEST(SevenSegmentOcr, ThresholdsTheImage) {
  const auto dim = [](Image &image) {
    for (uint8_t &pixel : image.pixels) {
      if (pixel == 255)
        pixel = 100;
    }
  };
  Image image = render("1");
  dim(image);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1).complete());

  image = render("1");
  dim(image);
  ocr.set_threshold_level(50);
  EXPECT_TRUE(ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1).complete());
}

TEST(SevenSegmentOcr, RejectsInvalidReserveWidth) {
  SevenSegmentOCR ocr;
  EXPECT_FALSE(ocr.reserve(0));
  EXPECT_FALSE(ocr.reserve(-4));
}

TEST(SevenSegmentOcr, IgnoresRunsNarrowerThanAnyDigit) {
  Image image{std::vector<uint8_t>(static_cast<size_t>(NORMAL.width) * NORMAL.height, 0), NORMAL.width};
  fill_rect(image.pixels.data(), image.width, NORMAL, 5, 0, 5, NORMAL.height - 1);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, NORMAL.height, 1);
  EXPECT_EQ(result.digits, 0);
  EXPECT_FALSE(result.complete());
}

TEST(SevenSegmentOcr, SaturatesTheDigitRunCount) {
  Image image = render(std::string(300, '1'), SMALL);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, SMALL.height, 1);
  EXPECT_EQ(result.digits, UINT8_MAX);
  EXPECT_FALSE(result.complete());
}

TEST(SevenSegmentOcr, ReportsTheThresholdLevel) {
  SevenSegmentOCR ocr;
  EXPECT_EQ(ocr.get_threshold_level(), 128);
  ocr.set_threshold_level(50);
  EXPECT_EQ(ocr.get_threshold_level(), 50);
}

}  // namespace esphome::seven_segment_ocr
