#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <cmath>

namespace esphome::gauge_needle_detector {

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

// Names stay in flash on ESP8266.
PROGMEM_STRING_TABLE(AlgorithmNames, "legacy", "radial_profile", "hough_transform", "template_match", "auto");

const LogString *algorithm_name(Algorithm algorithm) {
  return AlgorithmNames::get_log_str(algorithm, AlgorithmNames::LAST_INDEX);
}

namespace {

/// Sine and cosine of every whole degree, built once on first use.
struct TrigTables {
  std::array<float, 360> sine{};
  std::array<float, 360> cosine{};

  TrigTables() {
    for (size_t i = 0; i < this->sine.size(); i++) {
      const float radians = degrees_to_radians(static_cast<float>(i));
      this->sine[i] = std::sin(radians);
      this->cosine[i] = std::cos(radians);
    }
  }
};

const TrigTables &trig_tables() {
  static const TrigTables TABLES;
  return TABLES;
}

}  // namespace

float sin_lut(int degrees) { return trig_tables().sine[degrees]; }

float cos_lut(int degrees) { return trig_tables().cosine[degrees]; }

NeedleDetector::NeedleDetector() {
#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN
  this->detectors_[this->detector_count_++] = &this->intensity_scan_;
#endif
#ifdef USE_GAUGE_NEEDLE_RADIAL_PROFILE
  this->detectors_[this->detector_count_++] = &this->radial_profile_;
#endif
#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM
  this->detectors_[this->detector_count_++] = &this->hough_transform_;
#endif
#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH
  this->detectors_[this->detector_count_++] = &this->template_match_;
#endif
}

void NeedleDetector::reserve(int width, int height) {
  this->preprocessor_.reserve(width, height);
#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM
  this->hough_transform_.reserve(width, height);
#endif
}

DetectionResult NeedleDetector::detect(const uint8_t *img, int width, int height, const DetectorConfig &config) {
  if (img == nullptr || width <= 0 || height <= 0)
    return {};

  // "auto" and debug logging run every compiled-in detector.
  const bool compare_all = config.algorithm == ALGORITHM_AUTO || config.debug;

  // Preprocess once for all detectors that will run on the filtered image.
  const int radius = std::min(width, height) / 2 - 2;
  bool needs_filtered = false;
  for (size_t i = 0; i < this->detector_count_; i++) {
    if (this->detectors_[i]->needs_filtered_image() &&
        (compare_all || config.algorithm == this->detectors_[i]->algorithm())) {
      needs_filtered = true;
      break;
    }
  }

  const uint8_t *filtered = nullptr;
  if (needs_filtered) {
    if (this->preprocessor_.prepare(img, width, height, radius, config.needle_type, config.debug)) {
      filtered = this->preprocessor_.data();
    } else {
      ESP_LOGE(TAG, "%s: preprocessing failed, skipping the filtered detectors", config.id);
    }
  }

  DetectionResult selected;
  bool have_selected = false;
  DetectionResult best;
  Algorithm best_algorithm = ALGORITHM_AUTO;
  bool have_best = false;

  for (size_t i = 0; i < this->detector_count_; i++) {
    Detector *detector = this->detectors_[i];
    const bool is_target = config.algorithm == detector->algorithm();
    if (!is_target && !compare_all)
      continue;

    const bool wants_filtered = detector->needs_filtered_image();
    if (wants_filtered && filtered == nullptr)
      continue;

    const DetectionResult result = detector->detect(img, wants_filtered ? filtered : nullptr, width, height, config);

    if (config.debug) {
      ESP_LOGD(TAG, "  %s: angle=%.1f deg, conf=%.2f", LOG_STR_ARG(algorithm_name(detector->algorithm())), result.angle,
               result.confidence);
    }
    if (is_target) {
      selected = result;
      have_selected = true;
    }
    // The first detector wins ties, so the built-in order decides equal scores.
    if (config.algorithm == ALGORITHM_AUTO && (!have_best || result.confidence > best.confidence)) {
      best = result;
      best_algorithm = detector->algorithm();
      have_best = true;
    }
  }

  const bool is_auto = config.algorithm == ALGORITHM_AUTO;
  if (is_auto ? !have_best : !have_selected) {
    ESP_LOGE(TAG, "%s: no compiled-in detector for algorithm %s", config.id,
             LOG_STR_ARG(algorithm_name(config.algorithm)));
    return {};
  }

  const DetectionResult output = is_auto ? best : selected;
  const Algorithm output_algorithm = is_auto ? best_algorithm : config.algorithm;

  if (config.debug) {
    ESP_LOGD(TAG, "%s using %s: angle=%.1f deg, confidence=%.2f", config.id,
             LOG_STR_ARG(algorithm_name(output_algorithm)), output.angle, output.confidence);
  }

  return output;
}

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
