#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "esphome/components/seven_segment_ocr/seven_segment_ocr.h"

namespace esphome::seven_segment_ocr {
namespace {

constexpr int DIGIT_WIDTH = 60;
constexpr int DIGIT_HEIGHT = 90;
constexpr int DIGIT_GAP = 10;
constexpr int STROKE = 7;
// Stroke centres of a display whose vertical strokes sit at the outer edges.
constexpr int EDGE_LEFT = 3;
constexpr int EDGE_RIGHT = 56;
// Stroke centres of a display whose strokes are inset from the run edges.
constexpr int INSET_LEFT = 12;
constexpr int INSET_RIGHT = 48;
constexpr int SEGMENT_TOP = 13;
constexpr int SEGMENT_MIDDLE = 45;
constexpr int SEGMENT_BOTTOM = 76;

// Segment mask of each digit, in the engine's segment order.
constexpr uint8_t DIGIT_MASKS[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};

struct Image {
  std::vector<uint8_t> pixels;
  int width;
};

void fill_rect(uint8_t *image, int width, int x0, int y0, int x1, int y1) {
  for (int y = std::max(0, y0); y <= std::min(DIGIT_HEIGHT - 1, y1); y++) {
    for (int x = std::max(0, x0); x <= std::min(width - 1, x1); x++)
      image[y * width + x] = 255;
  }
}

void draw_segment(uint8_t *image, int width, int x0, int segment, int left_x, int right_x) {
  const int half = STROKE / 2;
  switch (segment) {
    case 0:  // A: top
      fill_rect(image, width, x0, SEGMENT_TOP - half, x0 + DIGIT_WIDTH - 1, SEGMENT_TOP + half);
      break;
    case 1:  // B: top right
      fill_rect(image, width, x0 + right_x - half, SEGMENT_TOP + 7, x0 + right_x + half, SEGMENT_MIDDLE - 5);
      break;
    case 2:  // C: bottom right
      fill_rect(image, width, x0 + right_x - half, SEGMENT_MIDDLE + 5, x0 + right_x + half, SEGMENT_BOTTOM - 6);
      break;
    case 3:  // D: bottom
      fill_rect(image, width, x0, SEGMENT_BOTTOM - half, x0 + DIGIT_WIDTH - 1, SEGMENT_BOTTOM + half);
      break;
    case 4:  // E: bottom left
      fill_rect(image, width, x0 + left_x - half, SEGMENT_MIDDLE + 5, x0 + left_x + half, SEGMENT_BOTTOM - 6);
      break;
    case 5:  // F: top left
      fill_rect(image, width, x0 + left_x - half, SEGMENT_TOP + 7, x0 + left_x + half, SEGMENT_MIDDLE - 5);
      break;
    case 6:  // G: middle
      fill_rect(image, width, x0, SEGMENT_MIDDLE - half, x0 + DIGIT_WIDTH - 1, SEGMENT_MIDDLE + half);
      break;
    default:
      break;
  }
}

Image render_mask(uint8_t mask, int left_x = EDGE_LEFT, int right_x = EDGE_RIGHT) {
  Image image{std::vector<uint8_t>(DIGIT_WIDTH * DIGIT_HEIGHT, 0), DIGIT_WIDTH};
  for (int segment = 0; segment < 7; segment++) {
    if ((mask & (1 << segment)) != 0)
      draw_segment(image.pixels.data(), image.width, 0, segment, left_x, right_x);
  }
  return image;
}

Image render(const std::string &digits, int left_x = EDGE_LEFT, int right_x = EDGE_RIGHT) {
  const int count = static_cast<int>(digits.size());
  const int width = count * DIGIT_WIDTH + (count - 1) * DIGIT_GAP;
  Image image{std::vector<uint8_t>(static_cast<size_t>(width) * DIGIT_HEIGHT, 0), width};
  for (int i = 0; i < count; i++) {
    const uint8_t mask = DIGIT_MASKS[digits[static_cast<size_t>(i)] - '0'];
    for (int segment = 0; segment < 7; segment++) {
      if ((mask & (1 << segment)) != 0)
        draw_segment(image.pixels.data(), image.width, i * (DIGIT_WIDTH + DIGIT_GAP), segment, left_x, right_x);
    }
  }
  return image;
}

}  // namespace

TEST(SevenSegmentOcr, ReadsEveryDigit) {
  for (char digit = '0'; digit <= '9'; digit++) {
    Image image = render(std::string(1, digit));
    SevenSegmentOCR ocr;
    ASSERT_TRUE(ocr.reserve(image.width));
    const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 1);
    EXPECT_TRUE(result.complete()) << "digit " << digit;
    EXPECT_EQ(result.value, static_cast<uint32_t>(digit - '0')) << "digit " << digit;
    EXPECT_EQ(result.unknown, 0);
    EXPECT_NEAR(result.confidence, 1.0f, 0.01f);
  }
}

TEST(SevenSegmentOcr, ReadsSeveralDigits) {
  Image image = render("120875");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 6);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.digits, 6);
  EXPECT_EQ(result.value, 120875u);
}

TEST(SevenSegmentOcr, ReadsDigitsWithInsetStrokes) {
  Image image = render("120875", INSET_LEFT, INSET_RIGHT);
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 6);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.value, 120875u);
  EXPECT_NEAR(result.confidence, 1.0f, 0.01f);
}

TEST(SevenSegmentOcr, StopsAtExpectedDigits) {
  Image image = render("12345678");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 4);
  EXPECT_TRUE(result.complete());
  EXPECT_EQ(result.digits, 4);
  EXPECT_EQ(result.value, 1234u);
}

TEST(SevenSegmentOcr, ReportsMissingDigits) {
  Image image = render("12");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 6);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 2);
  EXPECT_EQ(result.expected, 6);
  EXPECT_EQ(result.value, 12u);
}

TEST(SevenSegmentOcr, ReportsUnknownSegments) {
  Image image = render_mask(1 << 6);  // Only the middle segment, which is no digit.
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 1);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 1);
  EXPECT_EQ(result.unknown, 1);
  EXPECT_EQ(result.confidence, 0.0f);
}

TEST(SevenSegmentOcr, RejectsOverflowingValue) {
  Image image = render("9999999999");  // Ten nines do not fit in a uint32_t.
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 10);
  EXPECT_FALSE(result.complete());
  EXPECT_TRUE(result.overflow);
  EXPECT_EQ(result.digits, 10);
}

TEST(SevenSegmentOcr, ReadsLargestValue) {
  Image image = render("4294967295");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 10);
  EXPECT_TRUE(result.complete());
  EXPECT_FALSE(result.overflow);
  EXPECT_EQ(result.value, UINT32_MAX);
}

TEST(SevenSegmentOcr, RejectsUnreservedWidth) {
  Image image = render("1");
  SevenSegmentOCR ocr;
  EXPECT_TRUE(ocr.reserve(image.width - 1));
  const OcrResult result = ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 1);
  EXPECT_FALSE(result.complete());
  EXPECT_EQ(result.digits, 0);
}

TEST(SevenSegmentOcr, RejectsEmptyInput) {
  Image image = render("1");
  SevenSegmentOCR ocr;
  ASSERT_TRUE(ocr.reserve(image.width));
  EXPECT_FALSE(ocr.recognize(nullptr, image.width, DIGIT_HEIGHT, 1).complete());
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), 0, DIGIT_HEIGHT, 1).complete());
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), image.width, 0, 1).complete());
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 0).complete());
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
  EXPECT_FALSE(ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 1).complete());

  image = render("1");
  dim(image);
  ocr.set_threshold_level(50);
  EXPECT_TRUE(ocr.recognize(image.pixels.data(), image.width, DIGIT_HEIGHT, 1).complete());
}

}  // namespace esphome::seven_segment_ocr
