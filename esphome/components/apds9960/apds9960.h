#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace apds9960 {

class APDS9960 : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(red)
  SUB_SENSOR(green)
  SUB_SENSOR(blue)
  SUB_SENSOR(clear)
  SUB_SENSOR(proximity)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(up_direction)
  SUB_BINARY_SENSOR(right_direction)
  SUB_BINARY_SENSOR(down_direction)
  SUB_BINARY_SENSOR(left_direction)
#endif

 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;

  void set_led_drive(uint8_t level) { this->led_drive_ = level; }
  void set_proximity_gain(uint8_t gain) { this->proximity_gain_ = gain; }
  void set_ambient_gain(uint8_t gain) { this->ambient_gain_ = gain; }
  void set_gesture_led_drive(uint8_t level) { this->gesture_led_drive_ = level; }
  void set_gesture_gain(uint8_t gain) { this->gesture_gain_ = gain; }
  void set_gesture_wait_time(uint8_t wait_time) { this->gesture_wait_time_ = wait_time; }

 protected:
  bool is_color_enabled_() const;
  bool is_proximity_enabled_() const;
  bool is_gesture_enabled_() const;
  void read_color_data_(uint8_t status);
  void read_proximity_data_(uint8_t status);
  void read_gesture_data_();
  void report_gesture_(int gesture);
  void process_dataset_(int up, int down, int left, int right);

  uint8_t led_drive_;
  uint8_t proximity_gain_;
  uint8_t ambient_gain_;
  uint8_t gesture_led_drive_;
  uint8_t gesture_gain_;
  uint8_t gesture_wait_time_;

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
