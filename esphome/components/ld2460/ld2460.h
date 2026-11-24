#pragma once

#include "esphome/core/defines.h"
#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::ld2460 {

static constexpr uint8_t MAX_LINE_LENGTH = 32;  // Longest frame is a 5-target data frame of 31 bytes
static constexpr uint8_t MAX_TARGETS = 5;

enum class InstallationMode : uint8_t {
  INSTALLATION_MODE_SIDE = 1,
  INSTALLATION_MODE_TOP = 2,
};

class LD2460Component : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(target_count, uint8_t)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

#ifdef USE_SENSOR
  void set_target_x_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].x.set_sensor(s); }
  void set_target_y_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].y.set_sensor(s); }
  void set_target_distance_sensor(uint8_t target, sensor::Sensor *s) {
    this->target_sensors_[target].distance.set_sensor(s);
  }
  void set_target_angle_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].angle.set_sensor(s); }
#endif

  /// Add a callback that will be called after each successfully processed periodic data frame.
  template<typename F> void add_on_data_callback(F &&callback) { this->data_callback_.add(std::forward<F>(callback)); }

 protected:
  void send_query_();
  bool align_to_header_(const uint8_t *header);
  void handle_periodic_data_();
  void handle_ack_();
  void readline_(uint8_t readch);

#ifdef USE_SENSOR
  // INT16_MIN marks a slot with no published coordinates, so the next report always publishes
  struct TargetSensors {
    sensor::SensorWithDedup<float> x, y, distance, angle;
    int16_t last_x{INT16_MIN};
    int16_t last_y{INT16_MIN};
  };
  std::array<TargetSensors, MAX_TARGETS> target_sensors_{};
#endif

  LazyCallbackManager<void()> data_callback_;
  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint8_t buffer_pos_{0};
  // The angle formula depends on how the module is mounted, so it is read from the module at boot
  InstallationMode installation_mode_{InstallationMode::INSTALLATION_MODE_SIDE};
  // Queries sent so far; 0 once the module answered. Data frames resend up to MAX_QUERY_ATTEMPTS.
  uint8_t query_attempts_{1};
};

}  // namespace esphome::ld2460
