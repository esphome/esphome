#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
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
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_rad_1m_sensor(sensor::Sensor *rad_1m_sensor) { rad_1m_sensor_ = rad_1m_sensor; }
  void set_rad_10m_sensor(sensor::Sensor *rad_10m_sensor) { rad_10m_sensor_ = rad_10m_sensor; }
  void set_status_sensor(sensor::Sensor *status_sensor) { status_sensor_ = status_sensor; }
  void set_fw_version_sensor(sensor::Sensor *fw_version_sensor) { fw_version_sensor_ = fw_version_sensor; }
  void set_meas_time_sensor(sensor::Sensor *meas_time_sensor) { meas_time_sensor_ = meas_time_sensor; }
  void set_vibration_binary_sensor(binary_sensor::BinarySensor *vibration_binary_sensor) {
    vibration_binary_sensor_ = vibration_binary_sensor;
  }

 protected:
  bool read_bytes_with_retry_(uint8_t a_register, uint8_t *data, uint8_t len);
  bool reset_sensor_(uint8_t *data);
  bool read_dose_1m_(uint8_t *data);
  bool read_dose_10m_(uint8_t *data);
  bool read_status_(uint8_t *data);
  bool read_fw_version_(uint8_t *data);
  bool read_measuring_time_(uint8_t *data);

  sensor::Sensor *rad_1m_sensor_{nullptr};
  sensor::Sensor *rad_10m_sensor_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};
  sensor::Sensor *fw_version_sensor_{nullptr};
  sensor::Sensor *meas_time_sensor_{nullptr};
  binary_sensor::BinarySensor *vibration_binary_sensor_{nullptr};
};

}  // namespace gdk101
}  // namespace esphome
