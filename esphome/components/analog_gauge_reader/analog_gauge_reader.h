#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ANALOG_GAUGE_READER

#include "esphome/components/camera/camera.h"
#include "esphome/components/camera/decoder.h"
#include "esphome/components/gauge_needle_detector/gauge_needle_detector.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

namespace esphome {
namespace analog_gauge_reader {

/// Colour channel the dial is read from, or grayscale when the frame is luma.
enum ProcessChannel : uint8_t {
  PROCESS_CHANNEL_GRAYSCALE = 0,
  PROCESS_CHANNEL_RED = 1,
  PROCESS_CHANNEL_GREEN = 2,
  PROCESS_CHANNEL_BLUE = 3,
};

/// `target_color` value when the dial is read by brightness instead of a colour.
inline constexpr uint32_t NO_TARGET_COLOR = 0xFFFFFFFF;

struct DialConfig {
  /// Dial name used in logs. Points to a string literal.
  const char *id{""};
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
  float contrast{1.0f};  // Multiplier (1.0 = original)
  // Colour match tolerance (0 = legacy 0-442 linear map); off-colour pixels become background.
  float color_tolerance{0.0f};
  float min_scan_radius{0.3f};             // % of radius (0.0-1.0)
  float max_scan_radius{0.9f};             // % of radius (0.0-1.0)
  float deadzone_diameter{0.0f};           // Pixels (center circle ignored by detection)
  uint32_t target_color{NO_TARGET_COLOR};  // RGB hex, or NO_TARGET_COLOR
  sensor::Sensor *value_sensor{nullptr};
  sensor::Sensor *confidence_sensor{nullptr};
  sensor::Sensor *angle_sensor{nullptr};
  gauge_needle_detector::NeedleType needle_type{gauge_needle_detector::NEEDLE_TYPE_DARK};
  gauge_needle_detector::Algorithm algorithm{gauge_needle_detector::ALGORITHM_RADIAL_PROFILE};
  ProcessChannel process_channel{PROCESS_CHANNEL_GRAYSCALE};
  bool clockwise{true};      // Needle sweep direction (false = counter-clockwise)
  bool auto_contrast{true};  // Normalization (Min-Max Stretch)
  std::vector<std::pair<float, float>> calibration_mapping;
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

  // A calibration map wins over the angle range, including a wrap-around range.
  if (!dial.calibration_mapping.empty()) {
    const auto &points = dial.calibration_mapping;
    if (dial_angle <= points.front().first)
      return points.front().second;
    if (dial_angle >= points.back().first)
      return points.back().second;
    for (size_t i = 0; i < points.size() - 1; i++) {
      const float x1 = points[i].first;
      const float x2 = points[i + 1].first;
      if (dial_angle >= x1 && dial_angle <= x2) {
        if (std::abs(x2 - x1) < 0.001f)
          return points[i].second;
        const float t = (dial_angle - x1) / (x2 - x1);
        return points[i].second + t * (points[i + 1].second - points[i].second);
      }
    }
    return points.back().second;
  }

  // Measure from min_angle, clockwise, whatever the range.
  float span = dial.max_angle - dial.min_angle;
  if (span <= 0.0f)
    span += 360.0f;
  float delta = wrap360(dial_angle - dial.min_angle);
  // Off the scale the needle reads the nearer stop, so one before the start
  // reads the minimum instead of wrapping round to the maximum.
  if (delta > span) {
    const float past_end = delta - span;
    const float before_start = 360.0f - delta;
    delta = before_start <= past_end ? 0.0f : span;
  }
  const float fraction = delta / span;
  return dial.min_value + std::clamp(fraction, 0.0f, 1.0f) * (dial.max_value - dial.min_value);
}

/// Carry-corrects stacked dials and returns the combined value, or NAN.
/// The caller passes `count`-entry scratch arrays; `digits` is filled in reading order.
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
  if (!std::isfinite(steps) || std::abs(steps) > 1e15f)
    return NAN;
  const long long rounded_steps = static_cast<long long>(steps + (steps >= 0.0f ? 0.5f : -0.5f));

  // Derive the digits from the rounded total so the carry is consistent.
  for (size_t i = 0; i < count; i++) {
    const float ratio = readings[i].dial->scale / finest_scale;
    if (!std::isfinite(ratio) || std::abs(ratio) > 1e15f)
      continue;
    long long place = static_cast<long long>(ratio + (ratio >= 0.0f ? 0.5f : -0.5f));
    if (place <= 0)
      place = 1;
    const int digit = static_cast<int>((rounded_steps / place) % 10);
    digits[i] = digit < 0 ? digit + 10 : digit;
  }
  return static_cast<float>(rounded_steps) * finest_scale;
}

/// Reads dials out of esp32_camera frames: one shared decode, then one crop per dial.
class AnalogGaugeReader final : public PollingComponent, public esphome::camera::CameraListener {
 public:
  /// The camera, decoder and frame size never change, so they are constructor parameters.
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
  // Combine dials as odometer digits with carry correction instead of a weighted sum.
  void set_stacked_digits(bool enabled) { this->stacked_digits_ = enabled; }
  void add_dial(DialConfig config) { this->dials_.push_back(std::move(config)); }

 protected:
  /// How long a requested frame may take before the request is abandoned.
  static constexpr uint32_t FRAME_TIMEOUT_MS = 5000;
  /// Set by update(), cleared when the frame arrives or times out.
  bool requested_{false};
  uint32_t last_request_time_{0};
  /// One dial crop, allocated once in setup(); channels is 3 for colour, 1 otherwise.
  struct DialBuffer {
    std::unique_ptr<uint8_t[]> data;
    uint8_t channels{1};
  };
  bool paused_{false};
  bool stacked_digits_{false};
  esphome::camera::Camera *camera_{nullptr};
  esphome::camera::Decoder *decoder_{nullptr};
  int img_width_{0};
  int img_height_{0};
  gauge_needle_detector::NeedleDetector detector_;
  sensor::Sensor *value_sensor_{nullptr};
  std::vector<DialConfig> dials_;
  /// One crop buffer per entry in dials_, in the same order.
  std::vector<DialBuffer> dial_buffers_;
  bool debug_{false};
  std::shared_ptr<esphome::camera::CameraImage> pending_frame_{nullptr};
  std::unique_ptr<uint8_t[]> scratch_buffer_;
  std::vector<DialReading> readings_;
  std::vector<int> digits_;
  std::vector<float> phase_;
  std::vector<int> order_;
  void process_image_from_buffer(const uint8_t *data, size_t len);
  /// Copies a dial rectangle out of a frame, converting to luma when colour is not needed.
  bool crop_dial_(const uint8_t *src, const camera::CameraImageSpec &spec, const DialConfig &dial, DialBuffer &out);
};

}  // namespace analog_gauge_reader
}  // namespace esphome

#endif  // USE_ANALOG_GAUGE_READER
