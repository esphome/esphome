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
  void set_read_delay(uint32_t delay) { read_delay_ = delay; }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *capacitance_raw_{nullptr};
  uint32_t read_delay_;
  bool read_temp_c_(float &temp_c);
  bool read_capacitance_(uint16_t &touch_value);

 private:
  enum BaseAddress { STATUS = 0x00, TOUCH = 0x0F };
  enum TouchAddress { CHAN_0 = 0x10 };
  enum StatusAddress { HW_ID = 0x01, VERSION = 0x02, OPTIONS = 0x03, TEMPERATURE = 0x04, RESET = 0x7F };
  bool set_seesaw_port_(uint8_t base_address, uint8_t specific_address);
  bool busy_;
};

}  // namespace adafruit_soil_sensor
}  // namespace esphome
