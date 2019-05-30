#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace apds9960 {

class APDS9960 : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;

  void set_red_channel(sensor::Sensor *red_channel) { red_channel_ = red_channel; }
  void set_green_channel(sensor::Sensor *green_channel) { green_channel_ = green_channel; }
  void set_blue_channel(sensor::Sensor *blue_channel) { blue_channel_ = blue_channel; }
  void set_clear_channel(sensor::Sensor *clear_channel) { clear_channel_ = clear_channel; }
  void set_up_direction(binary_sensor::BinarySensor *up_direction) { up_direction_ = up_direction; }
  void set_right_direction(binary_sensor::BinarySensor *right_direction) { right_direction_ = right_direction; }
  void set_down_direction(binary_sensor::BinarySensor *down_direction) { down_direction_ = down_direction; }
  void set_left_direction(binary_sensor::BinarySensor *left_direction) { left_direction_ = left_direction; }
  void set_proximity(sensor::Sensor *proximity) { proximity_ = proximity; }

 protected:
  bool is_color_enabled_() const;
  bool is_proximity_enabled_() const;
  bool is_gesture_enabled_() const;
  void read_color_data_(uint8_t status);
  void read_proximity_data_(uint8_t status);
  void read_gesture_data_();
  void report_gesture_(int gesture);
  void process_dataset_(int up, int down, int left, int right);

  sensor::Sensor *red_channel_{nullptr};
  sensor::Sensor *green_channel_{nullptr};
  sensor::Sensor *blue_channel_{nullptr};
  sensor::Sensor *clear_channel_{nullptr};
  binary_sensor::BinarySensor *up_direction_{nullptr};
  binary_sensor::BinarySensor *right_direction_{nullptr};
  binary_sensor::BinarySensor *down_direction_{nullptr};
  binary_sensor::BinarySensor *left_direction_{nullptr};
  sensor::Sensor *proximity_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};
  bool gesture_up_started_{false};
  bool gesture_down_started_{false};
  bool gesture_left_started_{false};
  bool gesture_right_started_{false};
  uint32_t gesture_start_{0};
};

}  // namespace apds9960
}  // namespace esphome
