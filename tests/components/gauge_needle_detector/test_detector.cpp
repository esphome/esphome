#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/components/gauge_needle_detector/gauge_needle_detector.h"

namespace esphome::gauge_needle_detector::testing {

namespace {

constexpr int WIDTH = 64;
constexpr int HEIGHT = 64;
constexpr float CENTER_X = WIDTH / 2.0f;
constexpr float CENTER_Y = HEIGHT / 2.0f;
constexpr int NEEDLE_LENGTH = 30;
constexpr int THICK_NEEDLE = 3;

/// Draws a needle of `thickness` pixels from the centre, so a bar wider than one
/// pixel can be tested as well.
void draw_needle(std::vector<uint8_t> &crop, float degrees, int length, int thickness, uint8_t value) {
  const float rad = degrees_to_radians(degrees);
  const float dx = std::cos(rad);
  const float dy = std::sin(rad);
  for (int r = 0; r < length; r++) {
    for (int step = 0; step < thickness; step++) {
      const float offset = static_cast<float>(step) - (thickness - 1) / 2.0f;
      const int x = static_cast<int>(CENTER_X + r * dx - offset * dy);
      const int y = static_cast<int>(CENTER_Y + r * dy + offset * dx);
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        crop[static_cast<size_t>(y) * WIDTH + x] = value;
    }
  }
}

/// Crop of value `background` with a needle of value `needle`.
std::vector<uint8_t> make_plain_crop(float degrees, int background, int needle, int length = NEEDLE_LENGTH,
                                     int thickness = 1) {
  std::vector<uint8_t> crop(static_cast<size_t>(WIDTH) * HEIGHT, static_cast<uint8_t>(background));
  draw_needle(crop, degrees, length, thickness, static_cast<uint8_t>(needle));
  return crop;
}

/// Gradient background with a needle, so the contrast stretch has something to do.
std::vector<uint8_t> make_textured_crop(float degrees, NeedleType needle_type, int length = NEEDLE_LENGTH,
                                        int thickness = 1) {
  std::vector<uint8_t> crop(static_cast<size_t>(WIDTH) * HEIGHT);
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++)
      crop[static_cast<size_t>(y) * WIDTH + x] = static_cast<uint8_t>(40 + (x + y) * 80 / (WIDTH + HEIGHT));
  }
  draw_needle(crop, degrees, length, thickness, needle_type == NEEDLE_TYPE_LIGHT ? 255 : 0);
  return crop;
}

DetectorConfig make_config(Algorithm algorithm, NeedleType needle_type) {
  DetectorConfig config;
  config.id = "test";
  config.algorithm = algorithm;
  config.needle_type = needle_type;
  return config;
}

/// Smallest difference between two angles, in degrees.
float angle_difference(float a, float b) {
  const float diff = std::fmod(std::fabs(a - b), 360.0f);
  return diff > 180.0f ? 360.0f - diff : diff;
}

DetectionResult run(const std::vector<uint8_t> &crop, Algorithm algorithm, NeedleType needle_type) {
  NeedleDetector detector;
  EXPECT_TRUE(detector.reserve(WIDTH, HEIGHT));
  return detector.detect(crop.data(), WIDTH, HEIGHT, make_config(algorithm, needle_type));
}

void expect_find(const std::vector<uint8_t> &crop, Algorithm algorithm, NeedleType needle_type, float expected,
                 float tolerance) {
  const DetectionResult result = run(crop, algorithm, needle_type);
  EXPECT_LE(angle_difference(result.angle, expected), tolerance)
      << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm)) << " returned " << result.angle << " degrees";
  EXPECT_GT(result.confidence, 0.0f) << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm))
                                     << " found no confidence";
}

}  // namespace

TEST(GaugeNeedleDetector, AlgorithmNames) {
  EXPECT_STREQ("legacy", LOG_STR_ARG(algorithm_name(ALGORITHM_LEGACY)));
  EXPECT_STREQ("radial_profile", LOG_STR_ARG(algorithm_name(ALGORITHM_RADIAL_PROFILE)));
  EXPECT_STREQ("hough_transform", LOG_STR_ARG(algorithm_name(ALGORITHM_HOUGH_TRANSFORM)));
  EXPECT_STREQ("template_match", LOG_STR_ARG(algorithm_name(ALGORITHM_TEMPLATE_MATCH)));
  EXPECT_STREQ("auto", LOG_STR_ARG(algorithm_name(ALGORITHM_AUTO)));
}

TEST(GaugeNeedleDetector, IntensityScanFindsRawNeedle) {
  for (const float angle : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 270.0f}) {
    SCOPED_TRACE(::testing::Message() << "angle " << angle);
    expect_find(make_plain_crop(angle, 0, 255), ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT, angle, 5.0f);
    expect_find(make_plain_crop(angle, 255, 0), ALGORITHM_LEGACY, NEEDLE_TYPE_DARK, angle, 5.0f);
  }
}

TEST(GaugeNeedleDetector, FilteredDetectorsFindTexturedNeedle) {
  for (const NeedleType needle_type : {NEEDLE_TYPE_LIGHT, NEEDLE_TYPE_DARK}) {
    SCOPED_TRACE(::testing::Message() << "needle type " << static_cast<int>(needle_type));
    const std::vector<uint8_t> crop = make_textured_crop(45.0f, needle_type);
    for (const Algorithm algorithm :
         {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
      expect_find(crop, algorithm, needle_type, 45.0f, 8.0f);
    }
  }
}

TEST(GaugeNeedleDetector, FilteredDetectorsFindNeedleAtEveryAngle) {
  // The quadrants are not symmetric: the rays are rounded to pixels, and the
  // radial interpolation wraps around at 0 and 360 degrees.
  for (const float angle : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f, 350.0f}) {
    SCOPED_TRACE(::testing::Message() << "angle " << angle);
    for (const Algorithm algorithm :
         {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
      expect_find(make_plain_crop(angle, 0, 255), algorithm, NEEDLE_TYPE_LIGHT, angle, 8.0f);
      expect_find(make_plain_crop(angle, 255, 0), algorithm, NEEDLE_TYPE_DARK, angle, 8.0f);
    }
  }
}

TEST(GaugeNeedleDetector, FilteredDetectorsKeepThickNeedles) {
  // A three-pixel-wide needle is as thin as a 3x3 opening leaves alone; the
  // top-hat then subtracts it and the filtered detectors found nothing.
  for (const float angle : {0.0f, 90.0f}) {
    SCOPED_TRACE(::testing::Message() << "angle " << angle);
    for (const Algorithm algorithm :
         {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
      expect_find(make_plain_crop(angle, 0, 255, NEEDLE_LENGTH, THICK_NEEDLE), algorithm, NEEDLE_TYPE_LIGHT, angle,
                  8.0f);
      expect_find(make_plain_crop(angle, 255, 0, NEEDLE_LENGTH, THICK_NEEDLE), algorithm, NEEDLE_TYPE_DARK, angle,
                  8.0f);
    }
  }
}

TEST(GaugeNeedleDetector, BlankCropHasNoConfidence) {
  const std::vector<uint8_t> dark(static_cast<size_t>(WIDTH) * HEIGHT, 0);
  const std::vector<uint8_t> light(static_cast<size_t>(WIDTH) * HEIGHT, 255);
  for (const Algorithm algorithm : {ALGORITHM_LEGACY, ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM,
                                    ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
    EXPECT_FLOAT_EQ(0.0f, run(dark, algorithm, NEEDLE_TYPE_LIGHT).confidence)
        << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm));
    EXPECT_FLOAT_EQ(0.0f, run(light, algorithm, NEEDLE_TYPE_DARK).confidence)
        << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm));
  }
}

TEST(GaugeNeedleDetector, DetectRejectsBadInput) {
  const std::vector<uint8_t> crop = make_plain_crop(45.0f, 0, 255);
  NeedleDetector detector;
  ASSERT_TRUE(detector.reserve(WIDTH, HEIGHT));
  EXPECT_FLOAT_EQ(0.0f,
                  detector.detect(nullptr, WIDTH, HEIGHT, make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT)).confidence);
  EXPECT_FLOAT_EQ(0.0f,
                  detector.detect(crop.data(), 0, HEIGHT, make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT)).confidence);
  EXPECT_FLOAT_EQ(0.0f,
                  detector.detect(crop.data(), WIDTH, 0, make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT)).confidence);
}

TEST(GaugeNeedleDetector, CropsOutsideTheReservationAreRejected) {
  const std::vector<uint8_t> crop = make_plain_crop(45.0f, 0, 255);
  const DetectorConfig config = make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT);
  NeedleDetector detector;

  // Nothing is accepted before reserve() succeeds.
  EXPECT_FALSE(detector.reserve(0, HEIGHT));
  EXPECT_FLOAT_EQ(0.0f, detector.detect(crop.data(), WIDTH, HEIGHT, config).confidence);

  // A reservation smaller than the crop rejects it instead of reading past its end.
  ASSERT_TRUE(detector.reserve(WIDTH / 2, HEIGHT / 2));
  EXPECT_FLOAT_EQ(0.0f, detector.detect(crop.data(), WIDTH, HEIGHT, config).confidence);

  // A large enough reservation accepts the same crop afterwards.
  ASSERT_TRUE(detector.reserve(WIDTH, HEIGHT));
  EXPECT_GT(detector.detect(crop.data(), WIDTH, HEIGHT, config).confidence, 0.0f);
}

TEST(GaugeNeedleDetector, TinyCropDoesNotReportConfidence) {
  const std::vector<uint8_t> crop(16, 255);
  for (const Algorithm algorithm : {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH}) {
    NeedleDetector detector;
    ASSERT_TRUE(detector.reserve(4, 4));
    const DetectionResult result = detector.detect(crop.data(), 4, 4, make_config(algorithm, NEEDLE_TYPE_LIGHT));
    EXPECT_TRUE(std::isfinite(result.confidence));
    EXPECT_FLOAT_EQ(0.0f, result.confidence);
  }
}

TEST(GaugeNeedleDetector, HoughSupportRanksLongerNeedles) {
  // Preprocessing normalises intensity, so the number of edge pixels voting for
  // the winning angle is what separates a strong needle from a weak one.
  const DetectionResult weak = run(make_plain_crop(0.0f, 0, 255, 12), ALGORITHM_HOUGH_TRANSFORM, NEEDLE_TYPE_LIGHT);
  const DetectionResult strong =
      run(make_plain_crop(0.0f, 0, 255, NEEDLE_LENGTH), ALGORITHM_HOUGH_TRANSFORM, NEEDLE_TYPE_LIGHT);
  EXPECT_GT(weak.confidence, 0.0f);
  EXPECT_GT(strong.confidence, weak.confidence);
  // Both Sobel edges per radius step are budgeted for, so a clean needle does
  // not pin support at 1.
  EXPECT_LT(strong.confidence, 1.0f);
}

TEST(GaugeNeedleDetector, LegacyConfidenceNeedsRaySupport) {
  // One bright pixel inside the scan window is a lot of contrast but almost no
  // support, so it must not report the confidence of a needle that fills the ray.
  const DetectionResult single = run(make_plain_crop(0.0f, 0, 255, 10), ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT);
  const DetectionResult full = run(make_plain_crop(0.0f, 0, 255, NEEDLE_LENGTH), ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT);
  EXPECT_GT(single.confidence, 0.0f);
  EXPECT_LT(single.confidence, 0.2f);
  EXPECT_GT(full.confidence, 0.9f);
}

TEST(GaugeNeedleDetector, EmptyScanWindowHasNoConfidence) {
  const std::vector<uint8_t> crop = make_plain_crop(45.0f, 0, 255);
  NeedleDetector detector;
  ASSERT_TRUE(detector.reserve(WIDTH, HEIGHT));
  DetectorConfig config = make_config(ALGORITHM_RADIAL_PROFILE, NEEDLE_TYPE_LIGHT);
  config.deadzone_diameter = static_cast<float>(WIDTH);
  // The second call takes the already-reported path and must behave the same.
  for (int i = 0; i < 2; i++)
    EXPECT_FLOAT_EQ(0.0f, detector.detect(crop.data(), WIDTH, HEIGHT, config).confidence);
}

TEST(GaugeNeedleDetector, PreprocessorKeepsTheNeedle) {
  const std::vector<uint8_t> crop = make_textured_crop(45.0f, NEEDLE_TYPE_LIGHT);
  Preprocessor preprocessor;
  ASSERT_TRUE(preprocessor.reserve(WIDTH, HEIGHT));
  ASSERT_TRUE(preprocessor.prepare(crop.data(), WIDTH, HEIGHT, NEEDLE_LENGTH, NEEDLE_TYPE_LIGHT, false));
  ASSERT_NE(nullptr, preprocessor.data());

  // The one-pixel needle must survive contrast stretch and top-hat.
  size_t surviving = 0;
  const float rad = degrees_to_radians(45.0f);
  for (int r = 0; r < NEEDLE_LENGTH; r++) {
    const int x = static_cast<int>(CENTER_X + r * std::cos(rad));
    const int y = static_cast<int>(CENTER_Y + r * std::sin(rad));
    if (preprocessor.data()[static_cast<size_t>(y) * WIDTH + x] > 0)
      surviving++;
  }
  EXPECT_GT(surviving, static_cast<size_t>(NEEDLE_LENGTH) / 2);
}

TEST(GaugeNeedleDetector, PreprocessorRejectsCropsItCannotHold) {
  const std::vector<uint8_t> small = make_plain_crop(45.0f, 0, 255, 8);
  const std::vector<uint8_t> large = make_plain_crop(45.0f, 0, 255);
  Preprocessor preprocessor;
  ASSERT_TRUE(preprocessor.reserve(16, 16));

  // A crop larger than the reservation is refused, and the buffers stay usable.
  EXPECT_FALSE(preprocessor.prepare(large.data(), WIDTH, HEIGHT, NEEDLE_LENGTH, NEEDLE_TYPE_LIGHT, false));
  EXPECT_TRUE(preprocessor.prepare(small.data(), 16, 16, 8, NEEDLE_TYPE_LIGHT, false));
  EXPECT_NE(nullptr, preprocessor.data());

  // Growing the reservation accepts the large crop again.
  ASSERT_TRUE(preprocessor.reserve(WIDTH, HEIGHT));
  EXPECT_TRUE(preprocessor.prepare(large.data(), WIDTH, HEIGHT, NEEDLE_LENGTH, NEEDLE_TYPE_LIGHT, false));
}

}  // namespace esphome::gauge_needle_detector::testing
