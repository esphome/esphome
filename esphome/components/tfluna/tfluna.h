#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#include "esphome/components/i2c/i2c.h"

namespace esphome::tfluna {

class TFLuna : public i2c::I2CDevice, public PollingComponent {
#ifdef USE_SENSOR
  SUB_SENSOR(distance)
  SUB_SENSOR(temperature)
  SUB_SENSOR(signal_strength)
  SUB_SENSOR(timestamp)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(reset)
  SUB_BUTTON(restart)
#endif
 public:
  void setup() override;

  float get_setup_priority() const override { return setup_priority::LATE; }

  void dump_config() override;

  void update() override;

  void factory_reset();

  void restart();
};

}  // namespace esphome::tfluna
