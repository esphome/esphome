#include "m5ioe1_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/progmem.h"

namespace esphome::m5ioe1 {

static const char *TAG = "m5ioe1.sensor";

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG

PROGMEM_STRING_TABLE(ADCChannelStrings, "ADC Disabled", "ADC 1", "ADC 2", "ADC 3", "ADC 4", "Reference Voltage");

static const LogString *get_adc_channel_string(M5IOE1ADCChannel channel) {
  return ADCChannelStrings::get_log_str(static_cast<uint8_t>(channel), 0);
}

#endif

void M5IOE1ADCSensor::update() {
  float val = this->parent_->get_adc_value(this->channel_);
  this->publish_state(val);
}

void M5IOE1ADCSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5IOE1 ADC Sensor: \n"
                "  ADC Channel: %s",
                LOG_STR_ARG(get_adc_channel_string(this->channel_)));
  LOG_SENSOR(" ", "ADC Sensor: ", this);
}

void M5IOE1TemperatureSensor::update() {
  float val = this->parent_->get_temperature_value();
  this->publish_state(val);
}

}  // namespace esphome::m5ioe1
