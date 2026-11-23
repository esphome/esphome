#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include <vl53l4cx_class.h>

namespace esphome::vl53l4cx {

enum Target : uint8_t {
  TARGET_NEAREST,
  TARGET_FARTHEST,
  TARGET_STRONGEST,
};

/// The distance in mm of the valid target `target` picks, after `offset_mm`, ignoring targets closer than
/// `min_distance_mm`; -1 when there is none.
int32_t select_distance_mm(const VL53L4CX_MultiRangingData_t &data, Target target, uint16_t min_distance_mm,
                           int16_t offset_mm);

/// VL53L4CX multi-target time-of-flight sensor, ranged by ST's driver (esphome-libs/VL53L4CX) over this I2C device.
class VL53L4CXSensor final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice, public VL53L4CXHal {
 public:
  VL53L4CXSensor();

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_distance_mode(uint8_t mode) { this->distance_mode_ = mode; }
  void set_timing_budget(uint16_t budget_ms) { this->budget_ms_ = budget_ms; }
  void set_target(Target target) { this->target_ = target; }
  void set_min_distance(uint16_t min_distance_mm) { this->min_distance_mm_ = min_distance_mm; }
  void set_offset(int16_t offset_mm) { this->offset_mm_ = offset_mm; }

  // VL53L4CXHal: the driver passes the address on every call, since it changes when the sensor is re-addressed
  bool write_registers(uint8_t address, uint16_t reg, const uint8_t *data, uint16_t len) override;
  bool read_registers(uint8_t address, uint16_t reg, uint8_t *data, uint16_t len) override;
  uint32_t millis() override;
  void delay_ms(uint32_t ms) override;
  void delay_us(uint32_t us) override;

 protected:
  bool init_sensor_();
  bool start_measurement_();
  void fail_at_default_address_(const LogString *message);
  void measurement_failed_(const LogString *message, VL53L4CX_Error error);
  // A measurement that has not reported ready by now has failed
  uint32_t timeout_ms_() const { return static_cast<uint32_t>(this->budget_ms_) * 2 + 100; }
  void publish_result_();

  // Every instance, so the first setup() can hold all sensors with an enable pin in reset.
  static VL53L4CXSensor *first_sensor;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  VL53L4CX driver_{this};
  VL53L4CXSensor *next_{nullptr};
  GPIOPin *enable_pin_{nullptr};
  InternalGPIOPin *interrupt_pin_{nullptr};
  uint32_t measurement_start_{0};
  uint16_t budget_ms_{33};
  uint16_t min_distance_mm_{0};
  int16_t offset_mm_{0};
  uint8_t distance_mode_{VL53L4CX_DISTANCEMODE_LONG};
  Target target_{TARGET_NEAREST};
  uint8_t failures_{0};    // failed measurements in a row
  bool measuring_{false};  // a measurement was started and its result not yet read
};

}  // namespace esphome::vl53l4cx
