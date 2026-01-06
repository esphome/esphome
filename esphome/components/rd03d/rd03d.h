#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <array>

namespace esphome::rd03d {

static constexpr uint8_t MAX_TARGETS = 3;
static constexpr uint8_t FRAME_HEADER_SIZE = 4;
static constexpr uint8_t FRAME_FOOTER_SIZE = 2;
static constexpr uint8_t TARGET_DATA_SIZE = 8;
static constexpr uint8_t FRAME_SIZE =
    FRAME_HEADER_SIZE + (MAX_TARGETS * TARGET_DATA_SIZE) + FRAME_FOOTER_SIZE;  // 30 bytes

enum class TrackingMode : uint8_t {
  SINGLE_TARGET = 0,
  MULTI_TARGET = 1,
};

#ifdef USE_SENSOR
struct TargetSensor {
  sensor::Sensor *x{nullptr};
  sensor::Sensor *y{nullptr};
  sensor::Sensor *speed{nullptr};
  sensor::Sensor *distance{nullptr};
  sensor::Sensor *resolution{nullptr};
  sensor::Sensor *angle{nullptr};
};
#endif

class RD03DComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

#ifdef USE_SENSOR
  void set_target_count_sensor(sensor::Sensor *sensor) { this->target_count_sensor_ = sensor; }
  void set_x_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].x = sensor; }
  void set_y_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].y = sensor; }
  void set_speed_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].speed = sensor; }
  void set_distance_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].distance = sensor; }
  void set_resolution_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].resolution = sensor; }
  void set_angle_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].angle = sensor; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_target_binary_sensor(binary_sensor::BinarySensor *sensor) { this->target_binary_sensor_ = sensor; }
  void set_target_binary_sensor(uint8_t target, binary_sensor::BinarySensor *sensor) {
    this->target_presence_[target] = sensor;
  }
#endif

  // Configuration setters (called from code generation)
  void set_tracking_mode(TrackingMode mode) { this->tracking_mode_ = mode; }
  void set_throttle(uint32_t throttle) { this->throttle_ = throttle; }

 protected:
  void apply_config_();
  void send_command_(uint16_t command, const uint8_t *data = nullptr, uint8_t data_len = 0);
  void process_frame_();
#ifdef USE_SENSOR
  void publish_target_(uint8_t target_num, int16_t x, int16_t y, int16_t speed, uint16_t resolution);
#endif

#ifdef USE_SENSOR
  std::array<TargetSensor, MAX_TARGETS> targets_{};
  sensor::Sensor *target_count_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  std::array<binary_sensor::BinarySensor *, MAX_TARGETS> target_presence_{};
  binary_sensor::BinarySensor *target_binary_sensor_{nullptr};
#endif

  // Configuration (only sent if explicitly set)
  optional<TrackingMode> tracking_mode_{};
  uint32_t throttle_{0};
  uint32_t last_publish_time_{0};

  std::array<uint8_t, FRAME_SIZE> buffer_{};
  uint8_t buffer_pos_{0};
};

}  // namespace esphome::rd03d
