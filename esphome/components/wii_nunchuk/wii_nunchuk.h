#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor_with_dedup.h"
#endif

namespace esphome::wii_nunchuk {

class WiiNunchukComponent : public Component, public i2c::I2CDevice {
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(joystick_x, int16_t)
  SUB_SENSOR_WITH_DEDUP(joystick_y, int16_t)
  SUB_SENSOR_WITH_DEDUP(acceleration_x, int16_t)
  SUB_SENSOR_WITH_DEDUP(acceleration_y, int16_t)
  SUB_SENSOR_WITH_DEDUP(acceleration_z, int16_t)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(c_button)
  SUB_BINARY_SENSOR(z_button)
#endif

 public:
  void loop() override;
  void dump_config() override;

  // Called by register_component() from the update_interval option
  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }

 protected:
  bool init_();
  void read_state_();

  uint32_t update_interval_ms_{0};
  uint32_t last_poll_ms_{0};
  bool initialized_{false};
};

}  // namespace esphome::wii_nunchuk
