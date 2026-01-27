#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace adafruit_soil_sensor {

class AdafruitSoilSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_capacitance(sensor::Sensor *capacitance) { capacitance_raw_ = capacitance; }
  void set_calibration(uint16_t dry, uint16_t wet) {
    dry_ = dry;
    wet_ = wet;
  }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *capacitance_raw_{nullptr};
  uint16_t dry_;
  uint16_t wet_;
  bool read_(uint8_t reg_start, uint8_t reg_end, uint8_t *buf, uint16_t len);
  bool read_temp_c_(float &temp_c);
  bool read_capacitance_(uint16_t &touch_value);

 private:
  enum BaseAddress { STATUS = 0x00, TOUCH = 0x0F };
  enum TouchAddress { CHAN_0 = 0x10 };
  enum StatusAddress { HW_ID = 0x01, VERSION = 0x02, OPTIONS = 0x03, TEMPERATURE = 0x04, RESET = 0x7F };
};

}  // namespace adafruit_soil_sensor
}  // namespace esphome
