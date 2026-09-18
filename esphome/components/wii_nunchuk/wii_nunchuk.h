#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::wii_nunchuk {

class WiiNunchukComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }

#ifdef USE_SENSOR
  void set_joystick_x_sensor(sensor::Sensor *sensor) { this->joystick_x_sensor_ = sensor; }
  void set_joystick_y_sensor(sensor::Sensor *sensor) { this->joystick_y_sensor_ = sensor; }
  void set_acceleration_x_sensor(sensor::Sensor *sensor) { this->acceleration_x_sensor_ = sensor; }
  void set_acceleration_y_sensor(sensor::Sensor *sensor) { this->acceleration_y_sensor_ = sensor; }
  void set_acceleration_z_sensor(sensor::Sensor *sensor) { this->acceleration_z_sensor_ = sensor; }
#endif

#ifdef USE_BINARY_SENSOR
  void set_c_button_binary_sensor(binary_sensor::BinarySensor *sensor) { this->c_button_binary_sensor_ = sensor; }
  void set_z_button_binary_sensor(binary_sensor::BinarySensor *sensor) { this->z_button_binary_sensor_ = sensor; }
#endif

 protected:
  bool read_state_();
  void publish_state_() const;
  uint32_t decode_ident_(const uint8_t *ident) const;

  uint32_t update_interval_ms_{50};
  uint32_t last_update_ms_{0};
  bool setup_complete_{false};

  int16_t joystick_x_{0};
  int16_t joystick_y_{0};
  int16_t acceleration_x_{0};
  int16_t acceleration_y_{0};
  int16_t acceleration_z_{0};
  bool button_c_{false};
  bool button_z_{false};

#ifdef USE_SENSOR
  sensor::Sensor *joystick_x_sensor_{nullptr};
  sensor::Sensor *joystick_y_sensor_{nullptr};
  sensor::Sensor *acceleration_x_sensor_{nullptr};
  sensor::Sensor *acceleration_y_sensor_{nullptr};
  sensor::Sensor *acceleration_z_sensor_{nullptr};
#endif

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *c_button_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *z_button_binary_sensor_{nullptr};
#endif
};

}  // namespace esphome::wii_nunchuk
