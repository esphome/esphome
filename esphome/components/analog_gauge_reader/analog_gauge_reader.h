#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ANALOG_GAUGE_READER

#include "esphome/components/camera/camera.h"
#include "esphome/components/camera/decoder.h"
#include "esphome/components/gauge_needle_detector/gauge_needle_detector.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <utility>

namespace esphome::analog_gauge_reader {

/// Colour channel the dial is read from, or grayscale when the frame is luma.
enum ProcessChannel : uint8_t {
  PROCESS_CHANNEL_GRAYSCALE = 0,
  PROCESS_CHANNEL_RED = 1,
  PROCESS_CHANNEL_GREEN = 2,
  PROCESS_CHANNEL_BLUE = 3,
};

/// `target_color` value when the dial is read by brightness instead of a colour.
inline constexpr uint32_t NO_TARGET_COLOR = 0xFFFFFFFF;
/// Largest distance between two RGB colours, sqrt(3 * 255^2).
inline constexpr float MAX_COLOR_DISTANCE = 442.0f;
/// Beyond this many steps the stacked total is no longer exact in a float.
inline constexpr float MAX_EXACT_STEPS = 1e15f;

struct DialConfig {
  /// Dial name used in logs. Points to a string literal.
  const char *id{""};
  /// Crop buffer, a static array from codegen: crop_w * crop_h * channels bytes.
  uint8_t *crop{nullptr};
  sensor::Sensor *value_sensor{nullptr};
  sensor::Sensor *confidence_sensor{nullptr};
  sensor::Sensor *angle_sensor{nullptr};
#ifdef ANALOG_GAUGE_READER_CALIBRATION_POINTS
  /// Sorted by raw angle; the first calibration_count entries are used.
  std::array<std::pair<float, float>, ANALOG_GAUGE_READER_CALIBRATION_POINTS> calibration{};
#endif
  float scale{1.0f};
  int crop_x{0};
  int crop_y{0};
  int crop_w{0};
  int crop_h{0};
  float min_angle{0.0f};     // Degrees
  float max_angle{360.0f};   // Degrees
  float angle_offset{0.0f};  // 0 = North, 90 = East
  float min_value{0.0f};
  float max_value{10.0f};
  float contrast{1.0f};                    // Multiplier (1.0 = original)
  float color_tolerance{0.0f};             // 0 = the whole colour range; off-colour pixels become background
  float min_scan_radius{0.3f};             // Fraction of the radius
  float max_scan_radius{0.9f};             // Fraction of the radius
  float deadzone_diameter{0.0f};           // Pixels (center circle ignored by detection)
  uint32_t target_color{NO_TARGET_COLOR};  // RGB hex, or NO_TARGET_COLOR
  gauge_needle_detector::NeedleType needle_type{gauge_needle_detector::NEEDLE_TYPE_DARK};
  gauge_needle_detector::Algorithm algorithm{gauge_needle_detector::ALGORITHM_RADIAL_PROFILE};
  ProcessChannel process_channel{PROCESS_CHANNEL_GRAYSCALE};
  uint8_t channels{1};  // 3 when the dial is read from colour, 1 otherwise
  uint8_t calibration_count{0};
  bool clockwise{true};      // Needle sweep direction (false = counter-clockwise)
  bool auto_contrast{true};  // Normalization (Min-Max Stretch)
};

/// One dial's reading, published only once the whole frame has been read.
struct DialReading {
  const DialConfig *dial;
  float value;
  float angle;
  float confidence;
};

/// Folds an angle into [0, 360); fmodf() alone keeps the sign of its input.
inline float wrap360(float degrees) {
  float wrapped = fmodf(degrees, 360.0f);
  return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
}

/// Maps a detector angle (0 = East, clockwise) to the dial's value.
inline float angle_to_value(float image_angle, const DialConfig &dial) {
  // Image angles start at East, dial angles at North, so rotate by 90 degrees.
  float dial_angle = wrap360(image_angle + 90.0f);
  // The offset is the dial's physical origin, so it applies before the mirror.
  dial_angle = wrap360(dial_angle - dial.angle_offset);
  if (!dial.clockwise)
    dial_angle = wrap360(-dial_angle);

#ifdef ANALOG_GAUGE_READER_CALIBRATION_POINTS
  // A calibration map wins over the angle range, including a wrap-around range.
  if (dial.calibration_count > 0) {
    const auto &points = dial.calibration;
    const size_t last = dial.calibration_count - 1;
    if (dial_angle <= points[0].first)
      return points[0].second;
    for (size_t i = 0; i < last; i++) {
      const float x1 = points[i].first;
      const float x2 = points[i + 1].first;
      if (dial_angle >= x1 && dial_angle <= x2) {
        if (std::abs(x2 - x1) < 0.001f)
          return points[i].second;
        const float t = (dial_angle - x1) / (x2 - x1);
        return points[i].second + t * (points[i + 1].second - points[i].second);
      }
    }
    return points[last].second;
  }
#endif

  // Measure clockwise from min_angle; off the scale the needle reads the nearer
  // stop, so one before the start reads the minimum, not the maximum.
  float span = dial.max_angle - dial.min_angle;
  if (span <= 0.0f)
    span += 360.0f;
  float delta = wrap360(dial_angle - dial.min_angle);
  if (delta > span) {
    const float past_end = delta - span;
    const float before_start = 360.0f - delta;
    delta = before_start <= past_end ? 0.0f : span;
  }
  const float fraction = delta / span;
  return dial.min_value + std::clamp(fraction, 0.0f, 1.0f) * (dial.max_value - dial.min_value);
}

#ifdef USE_ANALOG_GAUGE_READER_STACKED_DIGITS
/// Carry-corrects stacked dials and returns the combined value, or NAN.
inline float combine_stacked_digits(const DialReading *readings, size_t count, int *order, float *phase, int *digits) {
  auto wrap10 = [](float x) -> float {
    x = fmodf(x, 10.0f);
    return x < 0.0f ? x + 10.0f : x;
  };
  auto round_digit10 = [](float x) -> int {
    const int digit = static_cast<int>(floorf(x + 0.5f));
    return digit >= 10 ? digit - 10 : digit;
  };

  std::iota(order, order + count, 0);
  // The index tie-break keeps equal scales in config order.
  std::sort(order, order + count, [readings](int a, int b) {
    const float sa = readings[a].dial->scale;
    const float sb = readings[b].dial->scale;
    return sa < sb || (sa == sb && a < b);
  });

  for (size_t p = 0; p < count; p++) {
    const int i = order[p];
    const float raw = wrap10(readings[i].value);
    const float lower_fraction = p == 0 ? 0.0f : phase[order[p - 1]] / 10.0f;
    const int digit = round_digit10(wrap10(raw - lower_fraction));
    digits[i] = digit;
    phase[i] = p == 0 ? raw : wrap10(static_cast<float>(digit) + lower_fraction);
  }

  // Sum from the continuous phase, or 9.9 -> 0 loses the carry.
  const int finest_i = order[0];
  const float finest_scale = readings[finest_i].dial->scale;
  float total = phase[finest_i] * finest_scale;
  for (size_t p = 1; p < count; p++) {
    const int i = order[p];
    total += static_cast<float>(digits[i]) * readings[i].dial->scale;
  }

  // Round without llroundf(), which not every ESP-IDF target provides.
  const float steps = total / finest_scale;
  if (!std::isfinite(steps) || std::abs(steps) > MAX_EXACT_STEPS)
    return NAN;
  const long long rounded_steps = static_cast<long long>(steps + (steps >= 0.0f ? 0.5f : -0.5f));

  // Derive the digits from the rounded total so the carry is consistent.
  for (size_t i = 0; i < count; i++) {
    const float ratio = readings[i].dial->scale / finest_scale;
    if (!std::isfinite(ratio) || std::abs(ratio) > MAX_EXACT_STEPS)
      continue;
    long long place = static_cast<long long>(ratio + (ratio >= 0.0f ? 0.5f : -0.5f));
    if (place <= 0)
      place = 1;
    const int digit = static_cast<int>((rounded_steps / place) % 10);
    digits[i] = digit < 0 ? digit + 10 : digit;
  }
  return static_cast<float>(rounded_steps) * finest_scale;
}
#endif  // USE_ANALOG_GAUGE_READER_STACKED_DIGITS

/// Stretches the crop to the full 0-255 range.
inline void apply_auto_contrast(uint8_t *data, int size) {
  // Copy the extremes first: the iterators point into the buffer that is rewritten below.
  const auto [min_it, max_it] = std::minmax_element(data, data + size);
  const uint8_t min_value = *min_it;
  const uint8_t max_value = *max_it;
  if (max_value > min_value) {
    const float scale = 255.0f / static_cast<float>(max_value - min_value);
    for (int i = 0; i < size; i++)
      data[i] = static_cast<uint8_t>(static_cast<float>(data[i] - min_value) * scale);
  }
}

/// Scales the distance from mid-grey; a multiplier of 1 leaves the crop alone.
inline void apply_contrast(uint8_t *data, int size, float contrast) {
  if (std::abs(contrast - 1.0f) < 0.01f)
    return;
  for (int i = 0; i < size; i++)
    data[i] =
        static_cast<uint8_t>(std::clamp((static_cast<float>(data[i]) - 128.0f) * contrast + 128.0f, 0.0f, 255.0f));
}

/// Reads dials out of esp32_camera frames: one shared decode, then one crop per dial.
class AnalogGaugeReader final : public PollingComponent, public esphome::camera::CameraListener {
 public:
  AnalogGaugeReader(esphome::camera::Camera *camera, esphome::camera::Decoder *decoder, int frame_width,
                    int frame_height)
      : camera_(camera), decoder_(decoder), img_width_(frame_width), img_height_(frame_height) {}
  void setup() override;
  void update() override;
  void loop() override;
  void on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) override;
  void dump_config() override;
  void set_value_sensor(sensor::Sensor *s) { this->value_sensor_ = s; }
  void set_dial_value_sensor(size_t index, sensor::Sensor *s) { this->dials_[index].value_sensor = s; }
  void set_dial_confidence_sensor(size_t index, sensor::Sensor *s) { this->dials_[index].confidence_sensor = s; }
  void set_dial_angle_sensor(size_t index, sensor::Sensor *s) { this->dials_[index].angle_sensor = s; }
  void set_pause_processing(bool paused) { this->paused_ = paused; }
  void set_debug(bool debug) { this->debug_ = debug; }
  /// The largest dial crop, which the detector reserves its buffers for.
  void set_max_crop(int width, int height) {
    this->max_crop_w_ = width;
    this->max_crop_h_ = height;
  }
  void add_dial(DialConfig config) { this->dials_.push_back(std::move(config)); }
#ifdef USE_ANALOG_GAUGE_READER_COLOR
  /// Holds one converted crop; a static array from codegen, sized for the largest crop.
  void set_scratch_buffer(uint8_t *buffer) { this->scratch_buffer_ = buffer; }
#endif

 protected:
  static constexpr uint32_t FRAME_TIMEOUT_ID = 0;
  /// How long a requested frame may take before the request is abandoned.
  static constexpr uint32_t FRAME_TIMEOUT_MS = 5000;
  esphome::camera::Camera *camera_;
  esphome::camera::Decoder *decoder_;
  sensor::Sensor *value_sensor_{nullptr};
#ifdef USE_ANALOG_GAUGE_READER_COLOR
  uint8_t *scratch_buffer_{nullptr};
#endif
  std::shared_ptr<esphome::camera::CameraImage> pending_frame_;
  gauge_needle_detector::NeedleDetector detector_;
  StaticVector<DialConfig, ANALOG_GAUGE_READER_DIAL_COUNT> dials_;
  StaticVector<DialReading, ANALOG_GAUGE_READER_DIAL_COUNT> readings_;
#ifdef USE_ANALOG_GAUGE_READER_STACKED_DIGITS
  std::array<int, ANALOG_GAUGE_READER_DIAL_COUNT> digits_{};
  std::array<float, ANALOG_GAUGE_READER_DIAL_COUNT> phase_{};
  std::array<int, ANALOG_GAUGE_READER_DIAL_COUNT> order_{};
#endif
  int img_width_;
  int img_height_;
  int max_crop_w_{0};
  int max_crop_h_{0};
  /// A frame has been requested and has not arrived or timed out yet.
  bool requested_{false};
  bool paused_{false};
  bool debug_{false};
  /// Reads every dial of one decoded frame; true when the sensors were published.
  bool process_image_(const uint8_t *data, size_t len);
  /// Copies a dial rectangle out of a frame into its crop buffer, as luma unless the dial needs colour.
  bool crop_dial_(const uint8_t *src, const camera::CameraImageSpec &spec, const DialConfig &dial);
};

}  // namespace esphome::analog_gauge_reader

#endif  // USE_ANALOG_GAUGE_READER
