#include "battery.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "../board_pins.h"
#include "esphome/core/log.h"

// analogReadMilliVolts() is available from ESP32 Arduino 2.0.0 onwards and
// automatically uses the ADC calibration stored in eFuse.
#include <Arduino.h>
#include <array>
#include <utility>

namespace esphome {
namespace lilygo_t5_47_plus {

static const char *const TAG = "lilygo_t5_47_plus.battery";

// Piecewise linear approximation of a typical 1S LiPo discharge curve
// (tuned for LP103454, 3.7 V nominal, measured at ~200 mA / 0.1 C, 25 °C).
// Outer clamping is done by min_voltage_ / max_voltage_ (configurable).
// { voltage [V], state of charge [%] }
static const std::array<std::pair<float, float>, 10> VOLTAGE_CURVE = {{
    {4.20f, 100.0f},  // fully charged
    {4.10f, 92.0f},
    {4.00f, 83.0f},
    {3.90f, 72.0f},
    {3.80f, 57.0f},
    {3.70f, 41.0f},  // nominal voltage
    {3.60f, 24.0f},
    {3.50f, 13.0f},
    {3.30f, 4.0f},
    {3.00f, 0.0f},  // cutoff
}};

void LilygoT547PlusBattery::setup() {
  // Configure ADC pin as input (not strictly required, but explicit)
  pinMode(BATT_PIN, INPUT);
}

void LilygoT547PlusBattery::dump_config() {
  ESP_LOGCONFIG(TAG, "LilyGo T5 4.7\" Plus Battery Sensor:");
  ESP_LOGCONFIG(TAG, "  ADC pin: GPIO%d", BATT_PIN);
  ESP_LOGCONFIG(TAG, "  Voltage divider factor: %.3f", this->voltage_divider_);
  ESP_LOGCONFIG(TAG, "  Voltage range: %.2f V (0%%) – %.2f V (100%%)", this->min_voltage_, this->max_voltage_);
  LOG_SENSOR("  ", "Battery voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery level", this->battery_level_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

void LilygoT547PlusBattery::update() {
  float voltage = this->read_battery_voltage_();

  ESP_LOGD(TAG, "Battery voltage: %.3f V", voltage);

  if (this->battery_voltage_sensor_ != nullptr) {
    this->battery_voltage_sensor_->publish_state(voltage);
  }

  if (this->battery_level_sensor_ != nullptr) {
    float percent = voltage_to_percent_(voltage);
    ESP_LOGD(TAG, "Battery level: %.0f %%", percent);
    this->battery_level_sensor_->publish_state(percent);
  }
}

float LilygoT547PlusBattery::read_battery_voltage_() {
  // Enable POWER_EN – the voltage divider at BATT_PIN is only powered
  // when the POWER_EN signal is active.
  epd_poweron();

  // Short delay for a stable ADC reading (as per LilyGo demo code)
  delay(10);  // stabilise ADC

  // analogReadMilliVolts() uses eFuse calibration automatically (if available)
  uint32_t mv = analogReadMilliVolts(BATT_PIN);

  // Apply hardware voltage-divider factor, then convert mV → V.
  // Default 2.0 = 1:1 resistor divider. Adjust via voltage_divider config
  // option if the sensor reads low (e.g. due to a protection diode or
  // non-standard resistor values).
  float voltage = (static_cast<float>(mv) * this->voltage_divider_) / 1000.0f;

  // Clamp to configurable maximum (fully charged)
  voltage = std::min(voltage, this->max_voltage_);

  // Disable POWER_EN again to save power
  epd_poweroff_all();

  return voltage;
}

float LilygoT547PlusBattery::voltage_to_percent_(float voltage) const {
  // Clamp to configurable bounds
  if (voltage >= this->max_voltage_)
    return 100.0f;
  if (voltage <= this->min_voltage_)
    return 0.0f;

  // Clamp to curve table bounds (safety net for user-defined voltages outside table range)
  if (voltage >= VOLTAGE_CURVE.front().first)
    return VOLTAGE_CURVE.front().second;
  if (voltage <= VOLTAGE_CURVE.back().first)
    return VOLTAGE_CURVE.back().second;

  // Piecewise linear interpolation
  for (size_t i = 0; i + 1 < VOLTAGE_CURVE.size(); i++) {
    const float v_hi = VOLTAGE_CURVE[i].first;
    const float v_lo = VOLTAGE_CURVE[i + 1].first;
    if (voltage >= v_lo && voltage <= v_hi) {
      const float p_hi = VOLTAGE_CURVE[i].second;
      const float p_lo = VOLTAGE_CURVE[i + 1].second;
      float t = (voltage - v_lo) / (v_hi - v_lo);
      return p_lo + t * (p_hi - p_lo);
    }
  }

  return 0.0f;
}

}  // namespace lilygo_t5_47_plus
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
