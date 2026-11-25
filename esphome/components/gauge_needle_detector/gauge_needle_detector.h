#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace esphome::gauge_needle_detector {

/// Degree/radian conversion; functions because the Arduino cores define macros.
inline constexpr float degrees_to_radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0f; }
inline constexpr float radians_to_degrees(float radians) { return radians * 180.0f / std::numbers::pi_v<float>; }

/// Whether the needle is darker or brighter than the dial face.
enum NeedleType : uint8_t {
  NEEDLE_TYPE_DARK = 0,   ///< Needle darker than the dial face.
  NEEDLE_TYPE_LIGHT = 1,  ///< Needle brighter than the dial face.
};

/// Algorithm used to locate the needle in a dial crop.
enum Algorithm : uint8_t {
  ALGORITHM_LEGACY = 0,  ///< Raw intensity scan. The config value is "legacy".
  ALGORITHM_RADIAL_PROFILE = 1,
  ALGORITHM_HOUGH_TRANSFORM = 2,
  ALGORITHM_TEMPLATE_MATCH = 3,
  ALGORITHM_AUTO = 4,  ///< Runs every compiled-in detector and keeps the most confident one.
};

const LogString *algorithm_name(Algorithm algorithm);

struct DetectorConfig {
  const char *id{"dial"};       // Dial name used in log lines
  float min_scan_radius{0.3f};  // Scan window, as a fraction of the radius
  float max_scan_radius{0.9f};
  float deadzone_diameter{0.0f};  // Ignored centre circle, in pixels
  NeedleType needle_type{NEEDLE_TYPE_DARK};
  Algorithm algorithm{ALGORITHM_RADIAL_PROFILE};
  bool debug{false};  // Log every compiled-in detector's result
};

struct DetectionResult {
  float angle{0.0f};
  float confidence{0.0f};
};

/// The crop region every detector samples, computed once per frame.
struct ScanWindow {
  int width;
  int height;
  int cx;
  int cy;
  int start_r;
  int end_r;

  bool valid() const { return this->start_r < this->end_r; }
};

/// Sine and cosine of a whole degree, read from a flash-backed table.
float sin_lut(int degrees);
float cos_lut(int degrees);

/// Whole degrees a detection compares, one score per entry.
inline constexpr int ANGLE_COUNT = 360;

/// Confidence shared by the ray algorithms: the share of a ray the needle covers,
/// combined with how the best score compares to the worst.
inline float ray_confidence(float support, float best, float worst) {
  if (best <= 0.0f || best <= worst)
    return 0.0f;
  const float contrast = (best - worst) / best;
  return std::clamp(support * (0.5f + 0.5f * contrast), 0.0f, 1.0f);
}

/// Highest and lowest of the 360 ray scores, and the whole degree of the highest.
struct ScoreRange {
  int best_angle{0};
  float best{-FLT_MAX};
  float worst{FLT_MAX};
};

inline ScoreRange find_score_range(const float *scores) {
  ScoreRange range;
  for (int deg = 0; deg < ANGLE_COUNT; deg++) {
    if (scores[deg] > range.best) {
      range.best = scores[deg];
      range.best_angle = deg;
    }
    range.worst = std::min(range.worst, scores[deg]);
  }
  return range;
}

class Detector {
 public:
  virtual ~Detector() = default;

  virtual Algorithm algorithm() const = 0;

  virtual bool needs_filtered_image() const { return true; }

  /// Finds the needle angle; `filtered` is nullptr for a raw-crop detector and
  /// `scores` is the caller's 360-entry workspace.
  virtual DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, const ScanWindow &window,
                                 const DetectorConfig &config, float *scores) = 0;
};

/// Shared preprocessing (contrast stretch and top-hat filter).
class Preprocessor final {
 public:
  Preprocessor() = default;
  ~Preprocessor();
  Preprocessor(const Preprocessor &) = delete;
  Preprocessor &operator=(const Preprocessor &) = delete;
  Preprocessor(Preprocessor &&) = delete;
  Preprocessor &operator=(Preprocessor &&) = delete;

  /// Allocates the buffers once; false when they cannot be allocated, so
  /// prepare() fails instead of growing at runtime.
  bool reserve(int width, int height);

  bool prepare(const uint8_t *raw, int width, int height, int radial_pixels, NeedleType needle_type, bool debug);

  const uint8_t *data() const { return this->working_; }

 protected:
  void release_();

  uint8_t *working_{nullptr};
  uint8_t *scratch_{nullptr};
  uint8_t *scratch_2_{nullptr};
  size_t capacity_{0};
  /// Set once the oversized-crop error has been logged, so it is logged once.
  bool capacity_warned_{false};
  RAMAllocator<uint8_t> allocator_;
  std::array<uint32_t, 256> histogram_{};
};

#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN
/// Scores each ray of the raw crop by how much needle-coloured pixels it crosses.
class IntensityScanDetector final : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_LEGACY; }
  bool needs_filtered_image() const override { return false; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, const ScanWindow &window,
                         const DetectorConfig &config, float *scores) override;
};
#endif  // USE_GAUGE_NEEDLE_INTENSITY_SCAN

#ifdef USE_GAUGE_NEEDLE_RADIAL_PROFILE
/// Traces rays from the centre and keeps the one with the longest bright run.
class RadialProfileDetector final : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_RADIAL_PROFILE; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, const ScanWindow &window,
                         const DetectorConfig &config, float *scores) override;
};
#endif  // USE_GAUGE_NEEDLE_RADIAL_PROFILE

#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM
/// Votes every edge pixel into a radial histogram, weighted by edge magnitude.
class HoughTransformDetector final : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_HOUGH_TRANSFORM; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, const ScanWindow &window,
                         const DetectorConfig &config, float *scores) override;
};
#endif  // USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH
/// Searches ray angles for the brightest average line.
class TemplateMatchDetector final : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_TEMPLATE_MATCH; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, const ScanWindow &window,
                         const DetectorConfig &config, float *scores) override;
};
#endif  // USE_GAUGE_NEEDLE_TEMPLATE_MATCH

/// Picks a detector per dial, preprocesses once and dispatches to it.
class NeedleDetector final {
 public:
  NeedleDetector();
  NeedleDetector(const NeedleDetector &) = delete;
  NeedleDetector &operator=(const NeedleDetector &) = delete;
  NeedleDetector(NeedleDetector &&) = delete;
  NeedleDetector &operator=(NeedleDetector &&) = delete;

  /// Allocates the buffers once; false when a needed one cannot be allocated.
  bool reserve(int width, int height);

  DetectionResult detect(const uint8_t *img, int width, int height, const DetectorConfig &config);

 protected:
  static constexpr size_t MAX_DETECTORS = 4;

#ifdef USE_GAUGE_NEEDLE_PREPROCESSOR
  Preprocessor preprocessor_;
#endif
  std::array<Detector *, MAX_DETECTORS> detectors_{};
  size_t detector_count_{0};
  /// Shared angle-score workspace; the detectors run one at a time.
  std::array<float, ANGLE_COUNT> scores_{};
  /// Largest crop accepted by detect(); 0 until reserve() succeeds.
  int reserved_width_{0};
  int reserved_height_{0};
  /// One bit per kind of problem; each kind is logged once, for the first dial that hits it.
  static constexpr uint8_t REPORT_OVERSIZE = 1 << 0;
  static constexpr uint8_t REPORT_EMPTY_WINDOW = 1 << 1;
  static constexpr uint8_t REPORT_MISSING_DETECTOR = 1 << 2;
  uint8_t reported_{0};
  /// True the first time this kind of problem is seen.
  bool first_report_(uint8_t kind) {
    const bool first = (this->reported_ & kind) == 0;
    this->reported_ |= kind;
    return first;
  }

#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN
  IntensityScanDetector intensity_scan_;
#endif
#ifdef USE_GAUGE_NEEDLE_RADIAL_PROFILE
  RadialProfileDetector radial_profile_;
#endif
#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM
  HoughTransformDetector hough_transform_;
#endif
#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH
  TemplateMatchDetector template_match_;
#endif
};

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
