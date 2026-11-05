#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_BQ25820_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/progmem.h"
#endif
#include "esphome/core/helpers.h"

#include <array>
#include <cmath>

namespace esphome::bq25820 {

static constexpr size_t BQ25820_LIMIT_COUNT = 5;
static constexpr size_t BQ25820_CONTROL_COUNT = 7;
// TS_ADC this close to REGN means no NTC is connected
static constexpr uint16_t NTC_OPEN_CODE = 1020;

// Register values built by code generation into a PROGMEM table, in the order of the register tables in bq25820.cpp
struct BQ25820Config {
  // ICHG, IPRECHG, IAC_DPM, VAC_DPM and VAC_REV; 0 keeps the reset value
  std::array<uint16_t, BQ25820_LIMIT_COUNT> limits;
  // VFB_REG, timer, power path, MPPT, TS threshold, TS region and charger control
  std::array<uint8_t, BQ25820_CONTROL_COUNT> controls;
  // Amps per current ADC step, from the sense resistors
  float ibat_scale;
  float iac_scale;
};

// CHARGER_STATUS_1-3 and FAULT_STATUS (0x21-0x24), read together by every poll
using BQ25820StatusRegisters = std::array<uint8_t, 4>;
#ifdef USE_BQ25820_SENSOR
// ADC_CONTROL in the low byte of the first word, then the IAC, IBAT, VAC, VBAT, VSYS and TS ADCs (0x2B-0x38)
using BQ25820AdcRegisters = std::array<int16_t, 7>;
#else
// ADC_CONTROL (0x2B)
using BQ25820AdcRegisters = std::array<uint8_t, 1>;
#endif

#ifdef USE_TEXT_SENSOR
// CHARGE_STAT codes, then TS_STAT codes; undefined TS codes 5-7 fall back to index 5
PROGMEM_STRING_TABLE(BQ25820StatusStrings, "Not Charging", "Trickle", "Precharge", "Fast Charge", "Taper", "Unknown",
                     "Top-Off", "Done", "Normal", "Warm", "Cool", "Cold", "Hot");

/// Publishes string `first + (value & 7)`, only on change
inline void publish_status_text(text_sensor::TextSensor *sensor, uint8_t &last, uint8_t value, uint8_t first) {
  value &= 0x07;
  if (value == last)
    return;
  last = value;
  sensor->publish_state(BQ25820StatusStrings::get_progmem_str(first + value, 5));
}
#endif

class BQ25820Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  /// `config` points into PROGMEM
  BQ25820Component(const BQ25820Config *config, bool force_mppt_sweep)
      : config_(config), force_mppt_sweep_(force_mppt_sweep) {}

  void setup() override;
  void dump_config() override;
  void update() override;

#ifdef USE_BQ25820_SENSOR
  SUB_SENSOR(vbat)
  SUB_SENSOR(vsys)
  SUB_SENSOR(vac)
  SUB_SENSOR(ibat)
  SUB_SENSOR(iac)
  SUB_SENSOR(temp_in_c)
  // TS thermistor B-parameter fit: 1/T = a + b * ln(R_ntc / R_pullup), from code generation
  void set_ntc_calibration(float a, float b) {
    this->ntc_a_ = a;
    this->ntc_b_ = b;
  }
#endif
#ifdef USE_BQ25820_STATUS_PUBLISHER
  HUB_PUBLISHER(status, const BQ25820StatusRegisters &)
  HUB_PUBLISHER(dump, const char *)
#endif

 protected:
  BQ25820Config load_config_() const;
  bool watchdog_enabled_() const;
  bool write_config_registers_();
  /// Writes the configuration, then polls again once the ADC has converted
  bool configure_();
#ifdef USE_BQ25820_SENSOR
  void publish_adc_(const BQ25820AdcRegisters &adc);
  /// Temperature from TS_ADC, the TS pin voltage as a 10-bit share of REGN; NAN for a shorted or open NTC
  float ntc_temperature_(int16_t ts_adc) const {
    const uint16_t code = ts_adc & 0x3FF;
    if (code == 0 || code >= NTC_OPEN_CODE)
      return NAN;
    const float ratio = code / 1024.0f;
    return 1.0f / (this->ntc_a_ + this->ntc_b_ * logf(ratio / (1.0f - ratio))) - 273.15f;
  }

  float ntc_a_{NAN};
  float ntc_b_{NAN};
#endif

  const BQ25820Config *config_;
  bool force_mppt_sweep_;
  // Cleared by a configuration write until the delayed poll, so no poll publishes unconverted ADC readings
  bool adc_ready_{false};
  // A failed write may still have set ADC_EN, so the next poll writes the configuration again regardless
  bool rewrite_{false};
};

}  // namespace esphome::bq25820
