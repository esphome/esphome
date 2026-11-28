#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"

#include <vl53l4cx_class.h>

namespace esphome::vl53l4cx {

enum Target : uint8_t {
  TARGET_NEAREST,
  TARGET_FARTHEST,
  TARGET_STRONGEST,
};

/// Distance in mm of the chosen valid target at or beyond `min_distance_mm`, or -1.
int32_t select_distance_mm(const VL53L4CX_MultiRangingData_t &data, Target target, uint16_t min_distance_mm,
                           int16_t offset_mm);

/// VL53L4CX multi-target time-of-flight sensor, driven by ST's driver (esphome-libs/VL53L4CX).
class VL53L4CXSensor final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice, public VL53L4CXHal {
 public:
#ifdef USE_VL53L4CX_ENABLE_PIN
  VL53L4CXSensor();
#endif

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

#ifdef USE_VL53L4CX_ENABLE_PIN
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
#endif
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
#endif
  void set_distance_mode(uint8_t mode) { this->distance_mode_ = mode; }
  void set_timing_budget(uint16_t budget_ms) { this->budget_ms_ = budget_ms; }
  void set_target(Target target) { this->target_ = target; }
  void set_min_distance(uint16_t min_distance_mm) { this->min_distance_mm_ = min_distance_mm; }
  void set_offset(int16_t offset_mm) { this->offset_mm_ = offset_mm; }

  bool write_registers(uint8_t address, uint16_t reg, const uint8_t *data, uint16_t len) override;
  bool read_registers(uint8_t address, uint16_t reg, uint8_t *data, uint16_t len) override;
  uint32_t millis() override;
  void delay_ms(uint32_t ms) override;
  void delay_us(uint32_t us) override;

 protected:
  static constexpr uint8_t MAX_FAILURES = 5;

#ifdef USE_VL53L4CX_INTERRUPT_PIN
  // Inline: on RP2 a non-inline .time_critical function conflicts with inline ones in that section
  static void IRAM_ATTR ready_isr(VL53L4CXSensor *sensor) {
    sensor->ready_edge_ = true;
    sensor->enable_loop_soon_any_context();
  }
#endif

  /// Boots, addresses and starts the sensor; returns the failure reason or nullptr.
  const LogString *init_and_start_(bool at_boot);
  void reset_sensor_();
  /// Holds the sensor in reset when it has an enable pin.
  void hold_in_reset_();
  void reset_failed_(const LogString *reason);
  bool resetting_() const { return this->failures_ >= MAX_FAILURES; }
  bool start_measurement_();
  void measurement_failed_(const LogString *message, VL53L4CX_Error error);
  uint32_t timeout_ms_() const { return static_cast<uint32_t>(this->budget_ms_) * 2 + 100; }
  void publish_result_();

#ifdef USE_VL53L4CX_ENABLE_PIN
  static VL53L4CXSensor *first_sensor;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

  VL53L4CX driver_{this};
#ifdef USE_VL53L4CX_ENABLE_PIN
  VL53L4CXSensor *next_{nullptr};
  GPIOPin *enable_pin_{nullptr};
#endif
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  InternalGPIOPin *interrupt_pin_{nullptr};
#endif
  uint32_t measurement_start_{0};
  uint16_t budget_ms_{33};
  uint16_t min_distance_mm_{0};
  int16_t offset_mm_{0};
  uint8_t distance_mode_{VL53L4CX_DISTANCEMODE_LONG};
  Target target_{TARGET_NEAREST};
  uint8_t failures_{0};  // in a row; MAX_FAILURES + 1 once a failed reset was logged
  uint8_t settle_frames_{0};
  bool measuring_{false};
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  volatile bool ready_edge_{false};
#endif
};

}  // namespace esphome::vl53l4cx
