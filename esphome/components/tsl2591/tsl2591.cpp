#include "tsl2591.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tsl2591 {

static const char *const TAG = "tsl2591";

void TSL2591Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TSL2591...");
  if (!this->tsl2591_.begin()) {
    ESP_LOGD("ERROR", "Could not find a TSL2591 Sensor. Did you configure I2C with the correct address?");
    this->mark_failed();
    return;
  }
}

void TSL2591Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TSL2591:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TSL2591 failed earlier, during setup");
    return;
  }

  tsl2591Gain_t raw_gain = this->tsl2591_.getGain();
  int gain = 0;
  std::string gain_word = "unknown";
  switch (raw_gain) {
    case TSL2591_GAIN_MULTIPLIER_LOW:
      gain = 1;
      gain_word = "low";
      break;
    case TSL2591_GAIN_MULTIPLIER_MED:
      gain = 25;
      gain_word = "medium";
      break;
    case TSL2591_GAIN_MULTIPLIER_HIGH:
      gain = 428;
      gain_word = "high";
      break;
    case TSL2591_GAIN_MULTIPLIER_MAX:
      gain = 9876;
      gain_word = "maximum";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Gain: %dx (%s)", gain, gain_word.c_str());

  tsl2591IntegrationTime_t raw_timing = this->tsl2591_.getTiming();
  int timing_ms = (1 + raw_timing) * 100;
  ESP_LOGCONFIG(TAG, "  Integration Time: %d ms", timing_ms);
  LOG_SENSOR("  ", "Full spectrum:", this->full_spectrum_sensor_);
  LOG_SENSOR("  ", "Infrared:", this->infrared_sensor_);
  LOG_SENSOR("  ", "Visible:", this->visible_sensor_);
  LOG_SENSOR("  ", "Calculated lux:", this->calculated_lux_sensor_);

  LOG_UPDATE_INTERVAL(this);
}

void TSL2591Component::update() {
  if (!is_failed()) {
    uint32_t combined = this->getCombinedIlluminance();
    uint16_t visible = this->getIlluminance(TSL2591_SENSOR_CHANNEL_VISIBLE, combined);
    uint16_t infrared = this->getIlluminance(TSL2591_SENSOR_CHANNEL_INFRARED, combined);
    uint16_t full = this->getIlluminance(TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM, combined);
    float    lux = this->getCalculatedLux(full, infrared);
    ESP_LOGD(TAG, "Got illuminance: combined 0x%X, full %d, IR %d, vis %d. Calc lux: %f", combined, full, infrared,
             visible, lux);
    if (this->full_spectrum_sensor_ != nullptr)
      this->full_spectrum_sensor_->publish_state(full);
    if (this->infrared_sensor_ != nullptr)
      this->infrared_sensor_->publish_state(infrared);
    if (this->visible_sensor_ != nullptr)
      this->visible_sensor_->publish_state(visible);
    if (this->calculated_lux_sensor_ != nullptr)
      this->calculated_lux_sensor_->publish_state(lux);
    status_clear_warning();
  }
}

void TSL2591Component::set_infrared_sensor(sensor::Sensor *infrared_sensor) {
  this->infrared_sensor_ = infrared_sensor;
}
void TSL2591Component::set_visible_sensor(sensor::Sensor *visible_sensor) {
  this->visible_sensor_ = visible_sensor;
}
void TSL2591Component::set_full_spectrum_sensor(sensor::Sensor *full_spectrum_sensor) {
  this->full_spectrum_sensor_ = full_spectrum_sensor;
}
void TSL2591Component::set_calculated_lux_sensor(sensor::Sensor *calculated_lux_sensor) {
  this->calculated_lux_sensor_ = calculated_lux_sensor;
}

void TSL2591Component::set_integration_time(TSL2591IntegrationTime integration_time) {
  this->tsl2591_.setTiming((tsl2591IntegrationTime_t) integration_time);
}
void TSL2591Component::set_gain(TSL2591Gain gain) {
  this->tsl2591_.setGain((tsl2591Gain_t) gain);
}
float TSL2591Component::get_setup_priority() const {
  return setup_priority::DATA;
}

uint32_t TSL2591Component::get_combined_illuminance() {
  return this->tsl2591_.getFullLuminosity();
}
uint16_t TSL2591Component::get_illuminance(TSL2591SensorChannel channel) {
  return this->tsl2591_.getLuminosity(channel);
}
float TSL2591Component::get_calculated_lux(uint16_t full_spectrum, uint16_t infrared) {
  return this->tsl2591_.calculateLux(full_spectrum, infrared);
}
// logic cloned from Adafruit library
uint16_t TSL2591Component::get_illuminance(TSL2591SensorChannel channel, uint32_t combined_illuminance) {
  if (channel == TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM) {
    // Reads two byte value from channel 0 (visible + infrared)
    return (combined_illuminance & 0xFFFF);
  } else if (channel == TSL2591_SENSOR_CHANNEL_INFRARED) {
    // Reads two byte value from channel 1 (infrared)
    return (combined_illuminance >> 16);
  } else if (channel == TSL2591_SENSOR_CHANNEL_VISIBLE) {
    // Reads all and subtracts out just the visible!
    return ((combined_illuminance & 0xFFFF) - (combined_illuminance >> 16));
  }

  // unknown channel!
  return 0;
}


}  // namespace tsl2591
}  // namespace esphome
