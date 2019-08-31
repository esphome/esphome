#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace scd30 {

/// This class implements support for the Sensirion scd30 i2c GAS (VOC and CO2eq) sensors.
class SCD30Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool write_command_(uint16_t command);
  bool read_data_(uint16_t *data, uint8_t len);
  uint8_t sht_crc_(uint8_t data1, uint8_t data2);

  enum ErrorCode {
    COMMUNICATION_FAILED,
    FIRMWARE_IDENTIFICATION_FAILED,
    MEASUREMENT_INIT_FAILED,
    UNKNOWN
  } error_code_{UNKNOWN};

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace scd30
}  // namespace esphome
