#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif  // USE_SENSOR
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace gdk101 {

static const uint8_t GDK101_REG_READ_FIRMWARE = 0xB4;        // Firmware version
static const uint8_t GDK101_REG_RESET = 0xA0;                // Reset register - reading its value triggers reset
static const uint8_t GDK101_REG_READ_STATUS = 0xB0;          // Status register
static const uint8_t GDK101_REG_READ_MEASURING_TIME = 0xB1;  // Mesuring time
static const uint8_t GDK101_REG_READ_10MIN_AVG = 0xB2;       // Average radiation dose per 10 min
static const uint8_t GDK101_REG_READ_1MIN_AVG = 0xB3;        // Average radiation dose per 1 min

class GDK101Component : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(rad_1m)
  SUB_SENSOR(rad_10m)
  SUB_SENSOR(status)
  SUB_SENSOR(fw_version)
  SUB_SENSOR(measurement_duration)
#endif  // USE_SENSOR
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(vibration)
#endif  // USE_BINARY_SENSOR

 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  bool read_bytes_with_retry_(uint8_t a_register, uint8_t *data, uint8_t len);
  bool reset_sensor_(uint8_t *data);
  bool read_dose_1m_(uint8_t *data);
  bool read_dose_10m_(uint8_t *data);
  bool read_status_(uint8_t *data);
  bool read_fw_version_(uint8_t *data);
  bool read_measurement_duration_(uint8_t *data);
};

}  // namespace gdk101
}  // namespace esphome
