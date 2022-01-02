#include "Lilygot547Battery.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lilygo_t5_47_battery {

void Lilygot547Battery::setup() {}

void Lilygot547Battery::update() {
  epd_poweron();
  // wait for voltage to stabilise
  delay(100);
  Lilygot547Battery::update_battery_info();
  epd_poweroff();
}
void Lilygot547Battery::update_battery_info() {
  Lilygot547Battery::correct_adc_reference();
  // 36 main power supply ?
  // 35 battery ?
  uint16_t v = analogRead(36);
  double_t battery_voltage = ((double_t) v / 4095.0) * 2.0 * 3.3 * (this->vref / 1000.0);
  if (this->voltage != nullptr) {
    this->voltage->publish_state(battery_voltage);
  }
}

void Lilygot547Battery::correct_adc_reference() {
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type =
      esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    this->vref = adc_chars.vref;
  }
}

}  // namespace lilygo_t5_47_battery
}  // namespace esphome
