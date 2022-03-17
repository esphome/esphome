#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sen5x {

/// This class implements support for the Sensirion SEN5x i2c/UART Particulate Matter
/// PM1.0, PM2.5, PM4, PM10 Air Quality sensors.
class SEN5xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { pm_2_5_sensor_ = pm_2_5; }
  void set_pm_4_0_sensor(sensor::Sensor *pm_4_0) { pm_4_0_sensor_ = pm_4_0; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { pm_10_0_sensor_ = pm_10_0; }
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_voc_sensor(sensor::Sensor *voc) { voc_sensor_ = voc; }
  void set_nox_sensor(sensor::Sensor *nox) { nox_sensor_ = nox;}

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool write_command_(uint16_t command);
  bool read_data_(uint16_t *data, uint8_t len);
  uint8_t sht_crc_(uint8_t data1, uint8_t data2);
  char serial_number_[33] = {0};  /// Terminating NULL character
  uint16_t raw_firmware_version_;
  bool start_continuous_measurement_();
  uint8_t skipped_data_read_cycles_ = 0;
  bool initialized_{false};

  enum ErrorCode {
    COMMUNICATION_FAILED,
    FIRMWARE_VERSION_REQUEST_FAILED,
    FIRMWARE_VERSION_READ_FAILED,
    SERIAL_NUMBER_REQUEST_FAILED,
    SERIAL_NUMBER_READ_FAILED,
    MEASUREMENT_INIT_FAILED,
    UNKNOWN
  } error_code_{UNKNOWN};

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  sensor::Sensor *nox_sensor_{nullptr};
};

}  // namespace sen5x
}  // namespace esphome
