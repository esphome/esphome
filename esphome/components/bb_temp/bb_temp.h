#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "bb_temperature.h"

namespace esphome {
namespace bb_temp {

class BBTempComponent : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_sda_pin(int sda) { _sda_pin = sda; }
  void set_scl_pin(int scl) { _scl_pin = scl; }

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

 protected:
  BBTemp _bbt;  // bb_temperature class instance
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  int _sda_pin, _scl_pin;
  BBT_SAMPLE _bbtSamp;
};

}  // namespace bb_temp
}  // namespace esphome
