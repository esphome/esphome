#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace esphome::gauge_needle_detector {

/// Degree/radian conversion. Functions because the Arduino cores define
/// `DEG_TO_RAD` and `RAD_TO_DEG` as macros.
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

/// Returns the configuration name of an algorithm, for logging with LOG_STR_ARG().
const LogString *algorithm_name(Algorithm algorithm);

/// Parameters that change how one dial crop is analysed.
struct DetectorConfig {
  const char *id{"dial"};                    // Dial name used in log lines
  NeedleType needle_type{NEEDLE_TYPE_DARK};  // Dark needle or light (white) needle
  Algorithm algorithm{ALGORITHM_RADIAL_PROFILE};
  float min_scan_radius{0.3f};  // Scan window, as a fraction of the radius
  float max_scan_radius{0.9f};
  float deadzone_diameter{0.0f};  // Ignored centre circle, in pixels
  bool debug{false};              // Log every compiled-in detector's result
};

/// Result of one detection pass.
struct DetectionResult {
  float angle{0.0f};
  float confidence{0.0f};
};

/// Sine and cosine of a whole degree, from tables built on first use.
float sin_lut(int degrees);
float cos_lut(int degrees);

/// One needle-finding strategy; it holds no per-frame state.
class Detector {
 public:
  virtual ~Detector() = default;

  /// Algorithm this detector implements.
  virtual Algorithm algorithm() const = 0;

  /// False when the detector works on the raw crop and can skip preprocessing.
  virtual bool needs_filtered_image() const { return true; }

  /// Finds the needle angle in a width x height grayscale crop.
  /// @param raw      the crop as delivered by the camera
  /// @param filtered raw after Preprocessor::prepare(), or nullptr when
  ///                 needs_filtered_image() is false
  virtual DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, int width, int height,
                                 const DetectorConfig &config) = 0;
};

/// Shared preprocessing (CLAHE, top-hat filter, median filter). Buffers only grow.
class Preprocessor {
 public:
  /// Reserves the working buffers for crops of up to width x height pixels,
  /// so the first frame does not allocate.
  void reserve(int width, int height);

  /// @param radial_pixels dial radius in pixels, used to size the top-hat kernel
  bool prepare(const uint8_t *raw, int width, int height, int radial_pixels, NeedleType needle_type, bool debug);

  const uint8_t *data() const { return this->working_buffer_.data(); }

 protected:
  std::array<uint16_t, 256> histogram_{};
  std::vector<uint8_t> working_buffer_;
  std::vector<uint8_t> scratch_buffer_;
  std::vector<uint8_t> scratch_buffer_2_;
};

#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN
/// Scores each ray of the raw crop by how much needle-coloured pixels it crosses.
class IntensityScanDetector : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_LEGACY; }
  bool needs_filtered_image() const override { return false; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, int width, int height,
                         const DetectorConfig &config) override;

 protected:
  std::array<float, 360> scores_{};
};
#endif  // USE_GAUGE_NEEDLE_INTENSITY_SCAN

#ifdef USE_GAUGE_NEEDLE_RADIAL_PROFILE
/// Traces rays from the centre and keeps the one with the longest bright run.
class RadialProfileDetector : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_RADIAL_PROFILE; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, int width, int height,
                         const DetectorConfig &config) override;

 protected:
  std::array<float, 360> scores_{};
};
#endif  // USE_GAUGE_NEEDLE_RADIAL_PROFILE

#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM
/// Votes every edge pixel into a radial histogram, weighted by edge magnitude.
class HoughTransformDetector : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_HOUGH_TRANSFORM; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, int width, int height,
                         const DetectorConfig &config) override;
  /// Grows the edge map so a crop of width x height pixels does not allocate.
  void reserve(int width, int height);

 protected:
  std::array<int, 360> accumulator_{};
  /// Edge magnitude map, grown to the largest crop seen. Never shrinks, so
  /// alternating dial sizes do not reallocate.
  std::vector<uint8_t> edges_;
};
#endif  // USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH
/// Searches ray angles for the brightest average line.
class TemplateMatchDetector : public Detector {
 public:
  Algorithm algorithm() const override { return ALGORITHM_TEMPLATE_MATCH; }
  DetectionResult detect(const uint8_t *raw, const uint8_t *filtered, int width, int height,
                         const DetectorConfig &config) override;
};
#endif  // USE_GAUGE_NEEDLE_TEMPLATE_MATCH

/// Picks a detector per dial, preprocesses once and dispatches to it.
class NeedleDetector {
 public:
  NeedleDetector();

  /// Reserves the working buffers for crops of up to width x height pixels, so
  /// the first frame does not allocate.
  void reserve(int width, int height);

  /// Runs the configured algorithm on a width x height grayscale crop.
  DetectionResult detect(const uint8_t *img, int width, int height, const DetectorConfig &config);

 protected:
  /// One slot per built-in detector.
  static constexpr size_t MAX_DETECTORS = 4;

  Preprocessor preprocessor_;
  std::array<Detector *, MAX_DETECTORS> detectors_{};
  size_t detector_count_{0};

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
