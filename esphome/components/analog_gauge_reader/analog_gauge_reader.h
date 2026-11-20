#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ANALOG_GAUGE_READER

#include "esphome/components/camera/camera.h"
#include "esphome/components/camera/decoder.h"
#include "esphome/components/gauge_needle_detector/gauge_needle_detector.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <atomic>
#include <cstdint>
#include <memory>
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

struct DialConfig {
  /// Dial name used in logs. Points to a string literal.
  const char *id{""};
  gauge_needle_detector::NeedleType needle_type{gauge_needle_detector::NEEDLE_TYPE_DARK};
  gauge_needle_detector::Algorithm algorithm{gauge_needle_detector::ALGORITHM_RADIAL_PROFILE};
  float scale{1.0f};
  int crop_x{0};
  int crop_y{0};
  int crop_w{0};
  int crop_h{0};
  float min_angle{0.0f};     // Degrees
  float max_angle{360.0f};   // Degrees
  float angle_offset{0.0f};  // 0 = North, 90 = East
  bool clockwise{true};      // Needle sweep direction (false = counter-clockwise)
  float min_value{0.0f};
  float max_value{10.0f};
  bool auto_contrast{true};  // Normalization (Min-Max Stretch)
  float contrast{1.0f};      // Multiplier (1.0 = original)
  uint32_t target_color{0};  // RGB hex
  bool use_color{false};
  // Colour match tolerance (0 = legacy 0-442 linear map); off-colour pixels become background.
  float color_tolerance{0.0f};
  ProcessChannel process_channel{PROCESS_CHANNEL_GRAYSCALE};
  float min_scan_radius{0.3f};    // % of radius (0.0-1.0)
  float max_scan_radius{0.9f};    // % of radius (0.0-1.0)
  float deadzone_diameter{0.0f};  // Pixels (center circle ignored by detection)
  sensor::Sensor *value_sensor{nullptr};
  sensor::Sensor *confidence_sensor{nullptr};
  sensor::Sensor *angle_sensor{nullptr};
  std::vector<std::pair<float, float>> calibration_mapping;
};

struct DialReading {
  const DialConfig *dial;
  float value;
};

/// Reads dials out of esp32_camera frames: one shared decode, then one crop per dial.
class AnalogGaugeReader : public PollingComponent, public esphome::camera::CameraListener {
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
  void set_pause_processing(bool paused) { this->paused_ = paused; }
  void set_debug(bool debug) { this->debug_ = debug; }
  // Combine dials as odometer digits with carry correction instead of a weighted sum.
  void set_stacked_digits(bool enabled) { this->stacked_digits_ = enabled; }
  void add_dial(DialConfig config) { this->dials_.push_back(config); }

 protected:
  enum class FrameState : uint8_t { IDLE, REQUESTED, AVAILABLE, PROCESSING };
  std::atomic<FrameState> frame_state_{FrameState::IDLE};
  uint32_t last_request_time_{0};
  /// One dial crop, allocated once in setup(); channels is 3 for colour, 1 otherwise.
  struct DialBuffer {
    std::unique_ptr<uint8_t[]> data;
    size_t size{0};
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
  std::vector<uint8_t> scratch_buffer_;
  std::vector<uint8_t> scratch_buffer_2_;
  std::vector<DialReading> readings_;
  std::vector<int> digits_;
  std::vector<float> phase_;
  std::vector<int> order_;
  void process_image_from_buffer(const uint8_t *data, size_t len);
  /// Copies a dial rectangle out of a frame, converting to luma when colour is not needed.
  bool crop_dial_(const uint8_t *src, const camera::CameraImageSpec &spec, const DialConfig &dial, DialBuffer &out);
  [[nodiscard]] float angle_to_value(float angle, const DialConfig &dial);
  void debug_angle_calculation(float image_angle, const DialConfig &dial);
};

}  // namespace analog_gauge_reader
}  // namespace esphome

#endif  // USE_ANALOG_GAUGE_READER
