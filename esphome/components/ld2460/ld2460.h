#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"

#include <array>

namespace esphome::ld2460 {

using namespace ld24xx;

static constexpr uint8_t MAX_LINE_LENGTH = 64;  // Max frame length (data: 31, ack: 16) + margins
static constexpr uint8_t MAX_TARGETS = 5;       // LD2460 supports up to 5 targets

enum InstallationMode : uint8_t {
  MODE_SIDE = 1,
  MODE_TOP = 2,
};

enum Sensitivity : uint8_t {
  SENSITIVITY_HIGH = 1,
  SENSITIVITY_MEDIUM = 2,
  SENSITIVITY_LOW = 3,
};

// Target coordinate struct
struct Target {
  float x{0.0f};
  float y{0.0f};
  float distance{0.0f};
  float angle{0.0f};
};

class LD2460Component : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(target_count, uint8_t)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(installation_height)
  SUB_NUMBER(installation_angle)
  SUB_NUMBER(detection_distance)
  SUB_NUMBER(detection_angle_min)
  SUB_NUMBER(detection_angle_max)
#endif
#ifdef USE_SELECT
  SUB_SELECT(baud_rate)
  SUB_SELECT(installation_mode)
  SUB_SELECT(sensitivity)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(reporting)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(factory_reset)
  SUB_BUTTON(restart)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void read_all_info();
  void restart_and_read_all_info();
  void factory_reset();
  void restart();

  void set_reporting(bool enable);
  void set_baud_rate(const char *state);
  void set_installation_mode(const char *state);
  void set_sensitivity(const char *state);
  void set_installation_params(float height, float angle);
  void set_detection_range(float distance, float min_angle, float max_angle);
  float get_installation_height() const { return this->installation_height_; }
  float get_installation_angle() const { return this->installation_angle_; }
  float get_detection_distance() const { return this->detection_distance_; }
  float get_detection_angle_min() const { return this->detection_angle_min_; }
  float get_detection_angle_max() const { return this->detection_angle_max_; }

#ifdef USE_SENSOR
  void set_target_x_sensor(uint8_t target, sensor::Sensor *s) { this->target_x_sensors_[target].set_sensor(s); }
  void set_target_y_sensor(uint8_t target, sensor::Sensor *s) { this->target_y_sensors_[target].set_sensor(s); }
  void set_target_distance_sensor(uint8_t target, sensor::Sensor *s) {
    this->target_distance_sensors_[target].set_sensor(s);
  }
  void set_target_angle_sensor(uint8_t target, sensor::Sensor *s) { this->target_angle_sensors_[target].set_sensor(s); }
#endif

  /// Add a callback that will be called after each successfully processed periodic data frame.
  template<typename F> void add_on_data_callback(F &&callback) { this->data_callback_.add(std::forward<F>(callback)); }

 protected:
  void send_command_(uint8_t command, const uint8_t *data = nullptr, uint8_t data_len = 0);
  void handle_periodic_data_();
  bool handle_ack_data_();
  void readline_(int readch);

  void query_version_();
  void query_installation_params_();
  void query_installation_mode_();
  void query_detection_range_();
  void query_sensitivity_();

  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint8_t buffer_pos_{0};

  uint8_t installation_mode_{MODE_SIDE};
  uint8_t sensitivity_{SENSITIVITY_MEDIUM};
  float installation_height_{2.6f};
  float installation_angle_{30.0f};
  float detection_distance_{6.0f};
  float detection_angle_min_{-60.0f};
  float detection_angle_max_{60.0f};

  Target target_info_[MAX_TARGETS];

#ifdef USE_SENSOR
  std::array<SensorWithDedup<float>, MAX_TARGETS> target_x_sensors_{};
  std::array<SensorWithDedup<float>, MAX_TARGETS> target_y_sensors_{};
  std::array<SensorWithDedup<float>, MAX_TARGETS> target_distance_sensors_{};
  std::array<SensorWithDedup<float>, MAX_TARGETS> target_angle_sensors_{};
#endif

  LazyCallbackManager<void()> data_callback_;
};

}  // namespace esphome::ld2460
