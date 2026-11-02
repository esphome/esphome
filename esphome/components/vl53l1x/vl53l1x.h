#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome::vl53l1x {

// Values index the distance mode and timing budget tables in the .cpp.
enum DistanceMode : uint8_t {
  DISTANCE_MODE_SHORT = 0,
  DISTANCE_MODE_MEDIUM = 1,
  DISTANCE_MODE_LONG = 2,
};

class VL53L1XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  VL53L1XSensor();

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_distance_mode(DistanceMode mode) { this->distance_mode_ = mode; }
  void set_timing_budget(uint16_t budget_ms) { this->budget_ms_ = budget_ms; }
  // Rate in MCPS, stored as the register's 9.7 fixed point value.
  void set_signal_rate_limit(float limit_mcps) { this->signal_rate_limit_ = static_cast<uint16_t>(limit_mcps * 128); }
  void set_offset(int16_t offset_mm) { this->offset_mm_ = offset_mm; }
  void set_roi(uint8_t width, uint8_t height) {
    this->roi_width_ = width;
    this->roi_height_ = height;
  }
  void set_roi_center(uint8_t center) { this->roi_center_ = center; }

 protected:
  void fail_at_default_address_(const LogString *message);
  bool wait_for_boot_();
  bool configure_();
  bool start_ranging_();

  bool write_byte16_(uint16_t reg, uint8_t value);
  bool write_word16_(uint16_t reg, uint16_t value);
  bool read_byte16_(uint16_t reg, uint8_t *value);
  bool read_word16_(uint16_t reg, uint16_t *value);

  // Every instance, so the first setup() can hold all sensors with an enable pin in reset.
  static VL53L1XSensor *first_sensor;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  VL53L1XSensor *next_{nullptr};
  GPIOPin *enable_pin_{nullptr};
  uint32_t measurement_start_{0};
  uint16_t budget_ms_{50};
  uint16_t signal_rate_limit_{128};
  int16_t offset_mm_{0};
  uint8_t roi_width_{16};
  uint8_t roi_height_{16};
  uint8_t roi_center_{0};  // 0 selects the centre the way the ULD does
  DistanceMode distance_mode_{DISTANCE_MODE_LONG};
  bool first_result_{true};
};

}  // namespace esphome::vl53l1x
