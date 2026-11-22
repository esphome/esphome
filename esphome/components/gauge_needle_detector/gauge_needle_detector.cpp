#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>

namespace esphome::gauge_needle_detector {

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

// Names stay in flash on ESP8266.
PROGMEM_STRING_TABLE(AlgorithmNames, "legacy", "radial_profile", "hough_transform", "template_match", "auto");

const LogString *algorithm_name(Algorithm algorithm) {
  return AlgorithmNames::get_log_str(algorithm, AlgorithmNames::LAST_INDEX);
}

namespace {

/// Sine of 0 to 90 degrees; the other quadrants mirror these values.
constexpr std::array<float, 91> SINE_TABLE PROGMEM = {
    0.000000000f, 0.017452406f, 0.034899497f, 0.052335956f, 0.069756474f, 0.087155743f, 0.104528463f, 0.121869343f,
    0.139173101f, 0.156434465f, 0.173648178f, 0.190808995f, 0.207911691f, 0.224951054f, 0.241921896f, 0.258819045f,
    0.275637356f, 0.292371705f, 0.309016994f, 0.325568154f, 0.342020143f, 0.358367950f, 0.374606593f, 0.390731128f,
    0.406736643f, 0.422618262f, 0.438371147f, 0.453990500f, 0.469471563f, 0.484809620f, 0.500000000f, 0.515038075f,
    0.529919264f, 0.544639035f, 0.559192903f, 0.573576436f, 0.587785252f, 0.601815023f, 0.615661475f, 0.629320391f,
    0.642787610f, 0.656059029f, 0.669130606f, 0.681998360f, 0.694658370f, 0.707106781f, 0.719339800f, 0.731353702f,
    0.743144825f, 0.754709580f, 0.766044443f, 0.777145961f, 0.788010754f, 0.798635510f, 0.809016994f, 0.819152044f,
    0.829037573f, 0.838670568f, 0.848048096f, 0.857167301f, 0.866025404f, 0.874619707f, 0.882947593f, 0.891006524f,
    0.898794046f, 0.906307787f, 0.913545458f, 0.920504853f, 0.927183855f, 0.933580426f, 0.939692621f, 0.945518576f,
    0.951056516f, 0.956304756f, 0.961261696f, 0.965925826f, 0.970295726f, 0.974370065f, 0.978147601f, 0.981627183f,
    0.984807753f, 0.987688341f, 0.990268069f, 0.992546152f, 0.994521895f, 0.996194698f, 0.997564050f, 0.998629535f,
    0.999390827f, 0.999847695f, 1.000000000f,
};

/// Reads one table entry; on ESP8266 the table lives in flash and needs a copy.
inline float sine_at(int index) {
  float value;
  progmem_memcpy(&value, &SINE_TABLE[index], sizeof(value));
  return value;
}

}  // namespace

float sin_lut(int degrees) {
  degrees %= 360;
  if (degrees < 0)
    degrees += 360;
  if (degrees <= 90)
    return sine_at(degrees);
  if (degrees <= 180)
    return sine_at(180 - degrees);
  if (degrees <= 270)
    return -sine_at(degrees - 180);
  return -sine_at(360 - degrees);
}

float cos_lut(int degrees) { return sin_lut(degrees + 90); }

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

bool NeedleDetector::reserve(int width, int height) {
#ifdef USE_GAUGE_NEEDLE_PREPROCESSOR
  return this->preprocessor_.reserve(width, height);
#else
  // A legacy-only build never touches the filtered image.
  (void) width;
  (void) height;
  return true;
#endif
}

DetectionResult NeedleDetector::detect(const uint8_t *img, int width, int height, const DetectorConfig &config) {
  if (img == nullptr || width <= 0 || height <= 0)
    return {};

  const bool compare_all = config.algorithm == ALGORITHM_AUTO || config.debug;
  const bool is_auto = config.algorithm == ALGORITHM_AUTO;
  const int radius = std::min(width, height) / 2 - 2;

  // A crop too small for a radius has an invalid window, which the detectors
  // report as no needle; only a configured window can be worth a warning.
  ScanWindow window{width, height, width / 2, height / 2, 1, 0};
  if (radius > 0) {
    const int deadzone_r = static_cast<int>(std::max(0.0f, config.deadzone_diameter * 0.5f));
    window.start_r = std::max({1, static_cast<int>(radius * config.min_scan_radius), deadzone_r});
    window.end_r = static_cast<int>(radius * config.max_scan_radius);
    // The window never changes for a given config, so report it once per dial.
    if (!window.valid()) {
      if (this->empty_scan_dial_ != config.id) {
        ESP_LOGW(TAG, "%s: empty scan window (deadzone %d px, scan radius %d px); no needle can be found", config.id,
                 deadzone_r, window.end_r);
        this->empty_scan_dial_ = config.id;
      }
      return {};
    }
  }

  const uint8_t *filtered = nullptr;
  bool preprocessing_failed = false;
#ifdef USE_GAUGE_NEEDLE_PREPROCESSOR
  bool needs_filtered = false;
  for (size_t i = 0; i < this->detector_count_; i++) {
    if (this->detectors_[i]->needs_filtered_image() &&
        (compare_all || config.algorithm == this->detectors_[i]->algorithm())) {
      needs_filtered = true;
      break;
    }
  }
  if (needs_filtered) {
    // prepare() logs its own reason when the crop does not fit.
    if (this->preprocessor_.prepare(img, width, height, radius, config.needle_type, config.debug)) {
      filtered = this->preprocessor_.data();
    } else {
      preprocessing_failed = true;
    }
  }
#endif

  DetectionResult output;
  Algorithm output_algorithm = config.algorithm;
  bool have_output = false;
  bool target_compiled = is_auto && this->detector_count_ > 0;

  for (size_t i = 0; i < this->detector_count_; i++) {
    Detector *detector = this->detectors_[i];
    const bool is_target = config.algorithm == detector->algorithm();
    if (!is_target && !compare_all)
      continue;
    target_compiled |= is_target;

    const bool wants_filtered = detector->needs_filtered_image();
    if (wants_filtered && filtered == nullptr)
      continue;

    const DetectionResult result =
        detector->detect(img, wants_filtered ? filtered : nullptr, window, config, this->scores_.data());

    if (config.debug) {
      ESP_LOGD(TAG, "  %s: angle=%.1f deg, conf=%.2f", LOG_STR_ARG(algorithm_name(detector->algorithm())), result.angle,
               result.confidence);
    }
    // The first detector wins ties, so the built-in order decides equal scores.
    if (is_auto ? (!have_output || result.confidence > output.confidence) : is_target) {
      output = result;
      output_algorithm = detector->algorithm();
      have_output = true;
    }
  }

  if (!have_output) {
    // The build cannot change at runtime, so report each dial and algorithm once.
    const bool reported =
        this->missing_detector_dial_ == config.id && this->missing_detector_algorithm_ == config.algorithm;
    // Preprocessing already reported its own failure; do not blame a missing detector.
    if ((!preprocessing_failed || !target_compiled) && !reported) {
      ESP_LOGE(TAG, "%s: no compiled-in detector for algorithm %s", config.id,
               LOG_STR_ARG(algorithm_name(config.algorithm)));
      this->missing_detector_dial_ = config.id;
      this->missing_detector_algorithm_ = config.algorithm;
    }
    return {};
  }

  if (config.debug) {
    ESP_LOGD(TAG, "%s using %s: angle=%.1f deg, confidence=%.2f", config.id,
             LOG_STR_ARG(algorithm_name(output_algorithm)), output.angle, output.confidence);
  }

  return output;
}

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
