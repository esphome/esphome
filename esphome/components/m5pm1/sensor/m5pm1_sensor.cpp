#include "m5pm1_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::m5pm1 {

static const char *const TAG = "m5pm1.sensor";

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
PROGMEM_STRING_TABLE(ADCChannelStrings, "ADC Disabled", "ADC 1", "ADC 2");

static const LogString *get_adc_channel_string(M5PM1ADCChannel channel) {
  return ADCChannelStrings::get_log_str(static_cast<uint8_t>(channel), 0);
}
#endif

void M5PM1ADCSensor::setup() {
  if (this->pin_ == nullptr) {
    ESP_LOGE(TAG, "ADC pin is not configured");
    this->mark_failed();
    return;
  }

  this->pin_->setup();
  if (!this->parent_->set_pin_function(this->pin_->get_pin(), M5PM1PinFunction::M5PM1_PIN_FUNCTION_ALTERNATE)) {
    ESP_LOGE(TAG, "Failed to configure ADC GPIO function");
    this->mark_failed();
  }
}

void M5PM1ADCSensor::update() {
  uint16_t raw;
  if (!this->parent_->read_adc(this->channel_, &raw)) {
    this->publish_state(NAN);
    return;
  }
  this->publish_state(static_cast<float>(raw));
}

void M5PM1ADCSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "M5PM1 ADC Sensor:\n  ADC Channel: %s", LOG_STR_ARG(get_adc_channel_string(this->channel_)));
  LOG_SENSOR("  ", "ADC Sensor", this);
}

void M5PM1TemperatureSensor::update() {
  uint16_t raw;
  if (!this->parent_->read_adc(M5PM1ADCChannel::M5PM1_ADC_CHANNEL_TEMPERATURE, &raw)) {
    this->publish_state(NAN);
    return;
  }
  this->publish_state(static_cast<float>(raw));
}

void M5PM1VoltageSensor::update() {
  uint16_t millivolts;
  bool success;
  switch (this->type_) {
    case M5PM1VoltageType::M5PM1_VOLTAGE_TYPE_BATTERY:
      success = this->parent_->read_battery_voltage(&millivolts);
      break;
    case M5PM1VoltageType::M5PM1_VOLTAGE_TYPE_VIN:
      success = this->parent_->read_vin_voltage(&millivolts);
      break;
    case M5PM1VoltageType::M5PM1_VOLTAGE_TYPE_5VOUT:
      success = this->parent_->read_5vout_voltage(&millivolts);
      break;
    case M5PM1VoltageType::M5PM1_VOLTAGE_TYPE_VREF:
      success = this->parent_->read_vref_voltage(&millivolts);
      break;
    default:
      success = false;
      break;
  }

  if (!success) {
    this->publish_state(NAN);
    return;
  }
  this->publish_state(static_cast<float>(millivolts) / 1000.0f);
}

}  // namespace esphome::m5pm1
