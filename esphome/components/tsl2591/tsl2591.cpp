#include "tsl2591.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tsl2591 {

static const char *const TAG = "tsl2591";

void TSL2591Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TSL2591...");
  if (!tsl2591_.begin()) {
    ESP_LOGD("ERROR","Could not find a TSL2591 Sensor. Did you configure I2C?");
    mark_failed();
    disabled_ = true;
    return;
  }
}

void TSL2591Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TSL2591:");
  LOG_I2C_DEVICE(this);

  if (disabled_) {
    ESP_LOGE(TAG, "Communication with TSL2591 failed earlier, during setup");
    return;
  }

  tsl2591Gain_t raw_gain = tsl2591_.getGain();
  int gain = 0;
  switch (raw_gain) {
  case TSL2591_GAIN_MULTIPLIER_LOW:
    gain = 1;
    break;
  case TSL2591_GAIN_MULTIPLIER_MED:
    gain = 25;
    break;
  case TSL2591_GAIN_MULTIPLIER_HIGH:
    gain = 428;
    break;
  case TSL2591_GAIN_MULTIPLIER_MAX:
    gain = 9876;
    break;
  }
  ESP_LOGCONFIG(TAG, "  Gain: %dx", gain);

  tsl2591IntegrationTime_t raw_timing = tsl2591_.getTiming();
  int timing_ms = (1 + raw_timing) * 100;
  ESP_LOGCONFIG(TAG, "  Integration Time: %d ms", timing_ms);
  LOG_SENSOR("  ", "Visible", this->visible_sensor_);
  LOG_SENSOR("  ", "Infrared", this->infrared_sensor_);
  LOG_SENSOR("  ", "Full spectrum", this->full_spectrum_sensor_);

  LOG_UPDATE_INTERVAL(this);
}

void TSL2591Component::update() {
  if (!disabled_) {
    uint32_t combined = getCombinedIlluminance();
    uint16_t visible  = getIlluminance(TSL2591_SENSOR_CHANNEL_VISIBLE, combined);
    uint16_t infrared = getIlluminance(TSL2591_SENSOR_CHANNEL_INFRARED, combined);
    uint16_t full     = getIlluminance(TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM, combined);
    ESP_LOGD(TAG, "Got illuminance: combined 0x%X, visible %d, infrared %d, full spectrum %d", combined, visible, infrared, full);
    if (visible_sensor_ != nullptr)
      visible_sensor_->publish_state(visible);
    if (infrared_sensor_ != nullptr)
      infrared_sensor_->publish_state(infrared);
    if (full_spectrum_sensor_ != nullptr)
      full_spectrum_sensor_->publish_state(full);
    status_clear_warning();
  }
}

void TSL2591Component::set_infrared_sensor(sensor::Sensor *infrared_sensor) { this->infrared_sensor_ = infrared_sensor; }
void TSL2591Component::set_visible_sensor(sensor::Sensor *visible_sensor) { this->visible_sensor_ = visible_sensor; }
void TSL2591Component::set_full_spectrum_sensor(sensor::Sensor *full_spectrum_sensor) { this->full_spectrum_sensor_ = full_spectrum_sensor; }

void TSL2591Component::set_integration_time(TSL2591IntegrationTime integration_time) {
  tsl2591_.setTiming((tsl2591IntegrationTime_t)integration_time);
}
void TSL2591Component::set_gain(TSL2591Gain gain) { tsl2591_.setGain((tsl2591Gain_t)gain); }
float TSL2591Component::get_setup_priority() const { return setup_priority::DATA; }

uint32_t TSL2591Component::getCombinedIlluminance() {return tsl2591_.getFullLuminosity();}
uint16_t TSL2591Component::getIlluminance(TSL2591SensorChannel channel) {return tsl2591_.getLuminosity(channel);}

// logic cloned from Adafruit library
uint16_t TSL2591Component::getIlluminance(TSL2591SensorChannel channel, uint32_t combined_illuminance) {
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
