#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/helpers.h"

#include <array>

namespace esphome::bq25820 {

// WATCHDOG[5:4] in the timer control register
static constexpr uint8_t TIMER_CONTROL_WATCHDOG = 0x30;

// CHARGER_STATUS_1-3 and FAULT_STATUS (0x21-0x24), read together by every poll
using BQ25820StatusRegisters = std::array<uint8_t, 4>;

class BQ25820BinarySensor;
class BQ25820TextSensor;

class BQ25820Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  // Amps per current ADC step, from the sense resistors
  void set_ibat_scale(float scale) { this->ibat_scale_ = scale; }
  void set_iac_scale(float scale) { this->iac_scale_ = scale; }
  // Register codes and values built by codegen. A current code of 0 keeps the reset value.
  void set_charge_current_code(uint16_t code) { this->charge_current_code_ = code; }
  void set_precharge_current_code(uint16_t code) { this->precharge_current_code_ = code; }
  void set_input_current_limit_code(uint16_t code) { this->input_current_limit_code_ = code; }
  void set_input_voltage_limit_code(uint16_t code) { this->input_voltage_limit_code_ = code; }
  void set_reverse_system_voltage_code(uint16_t code) { this->reverse_system_voltage_code_ = code; }
  void set_charge_voltage_code(uint8_t code) { this->charge_voltage_code_ = code; }
  void set_timer_control(uint8_t value) { this->timer_control_ = value; }
  void set_charger_control(uint8_t value) { this->charger_control_ = value; }
  void set_mppt_control(uint8_t value) { this->mppt_control_ = value; }
  void set_ts_thresholds(uint8_t value) { this->ts_thresholds_ = value; }
  void set_ideal_diode_enabled(bool enabled) { this->ideal_diode_enabled_ = enabled; }
  void set_force_mppt_sweep(bool enabled) { this->force_mppt_sweep_ = enabled; }
  void set_temp_enabled(bool enabled) { this->temp_enabled_ = enabled; }

#ifdef USE_SENSOR
  SUB_SENSOR(vbat)
  SUB_SENSOR(vsys)
  SUB_SENSOR(vac)
  SUB_SENSOR(ibat)
  SUB_SENSOR(iac)
  SUB_SENSOR(temp_in_c)
#endif
#ifdef BQ25820_BINARY_SENSOR_COUNT
  void register_binary_sensor(BQ25820BinarySensor *sensor) { this->binary_sensors_.push_back(sensor); }
#endif
#ifdef BQ25820_TEXT_SENSOR_COUNT
  void register_text_sensor(BQ25820TextSensor *sensor) { this->text_sensors_.push_back(sensor); }
#endif

 protected:
  bool watchdog_enabled_() const { return (this->timer_control_ & TIMER_CONTROL_WATCHDOG) != 0; }
  bool write_config_registers_();
  void publish_adc_();

#ifdef BQ25820_BINARY_SENSOR_COUNT
  StaticVector<BQ25820BinarySensor *, BQ25820_BINARY_SENSOR_COUNT> binary_sensors_;
#endif
#ifdef BQ25820_TEXT_SENSOR_COUNT
  StaticVector<BQ25820TextSensor *, BQ25820_TEXT_SENSOR_COUNT> text_sensors_;
#endif
  float ibat_scale_{0.002f};
  float iac_scale_{0.002f};
  uint16_t charge_current_code_{0};
  uint16_t precharge_current_code_{0};
  uint16_t input_current_limit_code_{0};
  uint16_t input_voltage_limit_code_{0};
  uint16_t reverse_system_voltage_code_{0};
  uint8_t charge_voltage_code_{0};
  uint8_t timer_control_{0};
  uint8_t charger_control_{0};
  uint8_t mppt_control_{0};
  uint8_t ts_thresholds_{0};
  bool ideal_diode_enabled_ : 1 {false};
  bool force_mppt_sweep_ : 1 {false};
  bool temp_enabled_ : 1 {true};
  bool config_written_ : 1 {false};
};

}  // namespace esphome::bq25820
