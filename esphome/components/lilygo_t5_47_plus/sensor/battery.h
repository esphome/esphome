#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

// EPD driver functions epd_poweron() / epd_poweroff_all()
#include "../epd_driver.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace lilygo_t5_47_plus {

/**
 * Battery sensor for the LILYGO T5 4.7" Plus e-paper display.
 *
 * Reads the battery voltage via ADC:
 *   - ESP32-S3: GPIO14 (ADC2, channel 3)
 *   - ESP32 (original): GPIO36 (ADC1, channel 0)
 *
 * The hardware uses a voltage divider before BATT_PIN to scale the battery
 * voltage into the ADC input range:
 *   battery voltage = ADC reading × voltage_divider
 *
 * The default divider factor is 2.0 (1:1 resistor-divider, R1 = R2).
 * If your board has a protection diode or different resistors, calibrate by
 * measuring the actual battery voltage with a multimeter and computing:
 *   voltage_divider: <actual_V> / (<adc_mV> / 1000)
 * Example: 4.19 V measured, sensor reports 3.48 V with default factor 2.0:
 *   ADC reads 1740 mV  →  correct divider = 4.19 / 1.74 ≈ 2.41
 *
 * IMPORTANT: POWER_EN must be active before each ADC measurement because
 * the voltage divider circuit shares the same supply path as the e-paper
 * display. This class automatically enables the driver briefly and disables
 * it again after the measurement.
 */
class LilygoT547PlusBattery : public PollingComponent {
 public:
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { this->battery_voltage_sensor_ = sensor; }
  void set_battery_level_sensor(sensor::Sensor *sensor) { this->battery_level_sensor_ = sensor; }

  /** Minimum voltage (V) mapped to 0% — discharge cutoff. Default: 3.00 V. */
  void set_min_voltage(float min_voltage) { this->min_voltage_ = min_voltage; }
  /** Maximum voltage (V) mapped to 100% — fully charged. Default: 4.20 V. */
  void set_max_voltage(float max_voltage) { this->max_voltage_ = max_voltage; }
  /**
   * Hardware voltage-divider factor: battery_voltage = adc_mv / 1000 × factor.
   * Default 2.0 (1:1 resistor divider). Increase if the sensor reads low.
   */
  void set_voltage_divider(float divider) { this->voltage_divider_ = divider; }

  void setup() override;
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  /** Reads the raw ADC value and returns the calibrated battery voltage in volts. */
  float read_battery_voltage_();

  /**
   * Converts a battery voltage (V) to a state-of-charge (0–100 %).
   * Uses the built-in LiPo discharge curve, clamped to [min_voltage_, max_voltage_].
   */
  float voltage_to_percent_(float voltage) const;

  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};

  float min_voltage_{3.00f};     // discharge cutoff → 0 %
  float max_voltage_{4.20f};     // fully charged  → 100 %
  float voltage_divider_{2.0f};  // hardware voltage-divider factor (default: 1:1 = ×2)
};

}  // namespace lilygo_t5_47_plus
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
