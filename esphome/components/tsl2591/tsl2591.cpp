#include "tsl2591.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tsl2591 {

static const char *const TAG = "tsl2591.sensor";

// Various constants used in TSL2591 register manipulation
#define TSL2591_COMMAND_BIT (0xA0)      // 1010 0000: bits 7 and 5 for 'command normal'
#define TSL2591_ENABLE_POWERON (0x01)   // Flag for ENABLE register, to enable
#define TSL2591_ENABLE_POWEROFF (0x00)  // Flag for ENABLE register, to disable
#define TSL2591_ENABLE_AEN (0x02)       // Flag for ENABLE register, to turn on ADCs

// TSL2591 registers from the datasheet. We only define what we use.
#define TSL2591_REGISTER_ENABLE (0x00)
#define TSL2591_REGISTER_CONTROL (0x01)
#define TSL2591_REGISTER_DEVICE_ID (0x12)
#define TSL2591_REGISTER_STATUS (0x13)
#define TSL2591_REGISTER_CHAN0_LOW (0x14)
#define TSL2591_REGISTER_CHAN0_HIGH (0x15)
#define TSL2591_REGISTER_CHAN1_LOW (0x16)
#define TSL2591_REGISTER_CHAN1_HIGH (0x17)

void TSL2591Component::enable() {
  // Enable the device by setting the control bit to 0x01. Also turn on ADCs.
  this->write_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE, TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN);
}

void TSL2591Component::disable() {
  if (this->power_save_mode_enabled_) {
    // Disable the device by setting the control bit to 0x00
    this->write_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE, TSL2591_ENABLE_POWEROFF);
  }
}

void TSL2591Component::disable_internal_() {
  if (this->power_save_mode_enabled_) {
    this->disable();
  }
}

void TSL2591Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TSL2591...");
  auto id = this->read_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_DEVICE_ID);
  if (id != 0x50) {
    ESP_LOGD("ERROR", "Could not find a TSL2591 Sensor. Did you configure I2C with the correct address?");
    this->mark_failed();
  } else {
    this->disable_internal_();
  }
}

void TSL2591Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TSL2591:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TSL2591 failed earlier, during setup");
    return;
  }

  ESP_LOGCONFIG(TAG, "  Name: %s", this->name_);
  TSL2591Gain raw_gain = this->gain_;
  int gain = 0;
  std::string gain_word = "unknown";
  switch (raw_gain) {
    case TSL2591_GAIN_LOW:
      gain = 1;
      gain_word = "low";
      break;
    case TSL2591_GAIN_MED:
      gain = 25;
      gain_word = "medium";
      break;
    case TSL2591_GAIN_HIGH:
      gain = 400;
      gain_word = "high";
      break;
    case TSL2591_GAIN_MAX:
      gain = 9500;
      gain_word = "maximum";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Gain: %dx (%s)", gain, gain_word.c_str());
  TSL2591IntegrationTime raw_timing = this->integration_time_;
  int timing_ms = (1 + raw_timing) * 100;
  ESP_LOGCONFIG(TAG, "  Integration Time: %d ms", timing_ms);
  ESP_LOGCONFIG(TAG, "  Power save mode enabled: %s", this->power_save_mode_enabled_ ? "True" : "False");
  LOG_SENSOR("  ", "Full spectrum:", this->full_spectrum_sensor_);
  LOG_SENSOR("  ", "Infrared:", this->infrared_sensor_);
  LOG_SENSOR("  ", "Visible:", this->visible_sensor_);
  LOG_SENSOR("  ", "Calculated lux:", this->calculated_lux_sensor_);

  LOG_UPDATE_INTERVAL(this);
}

void TSL2591Component::update() {
  if (!is_failed()) {
    uint32_t combined = this->get_combined_illuminance();
    uint16_t visible = this->get_illuminance(TSL2591_SENSOR_CHANNEL_VISIBLE, combined);
    uint16_t infrared = this->get_illuminance(TSL2591_SENSOR_CHANNEL_INFRARED, combined);
    uint16_t full = this->get_illuminance(TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM, combined);
    float lux = this->get_calculated_lux(full, infrared);
    ESP_LOGD(TAG, "Got illuminance: combined 0x%X, full %d, IR %d, vis %d. Calc lux: %f", combined, full, infrared,
             visible, lux);
    if (this->full_spectrum_sensor_ != nullptr) {
      this->full_spectrum_sensor_->publish_state(full);
    }
    if (this->infrared_sensor_ != nullptr) {
      this->infrared_sensor_->publish_state(infrared);
    }
    if (this->visible_sensor_ != nullptr) {
      this->visible_sensor_->publish_state(visible);
    }
    if (this->calculated_lux_sensor_ != nullptr) {
      this->calculated_lux_sensor_->publish_state(lux);
    }
    status_clear_warning();
  }
}

void TSL2591Component::set_infrared_sensor(sensor::Sensor *infrared_sensor) {
  this->infrared_sensor_ = infrared_sensor;
}

void TSL2591Component::set_visible_sensor(sensor::Sensor *visible_sensor) { this->visible_sensor_ = visible_sensor; }

void TSL2591Component::set_full_spectrum_sensor(sensor::Sensor *full_spectrum_sensor) {
  this->full_spectrum_sensor_ = full_spectrum_sensor;
}

void TSL2591Component::set_calculated_lux_sensor(sensor::Sensor *calculated_lux_sensor) {
  this->calculated_lux_sensor_ = calculated_lux_sensor;
}

void TSL2591Component::set_integration_time(TSL2591IntegrationTime integration_time) {
  this->enable();
  this->integration_time_ = integration_time;
  this->write_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL, this->integration_time_ | this->gain_);  // NOLINT
  disable_internal_();
}

void TSL2591Component::set_gain(TSL2591Gain gain) {
  this->enable();
  this->gain_ = gain;
  this->write_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL, this->integration_time_ | this->gain_);  // NOLINT
  this->disable_internal_();
}

void TSL2591Component::set_power_save_mode(bool enable) { this->power_save_mode_enabled_ = enable; }

void TSL2591Component::set_name(const char *name) { this->name_ = name; }

float TSL2591Component::get_setup_priority() const { return setup_priority::DATA; }

boolean TSL2591Component::is_adc_valid() {
  uint8_t status;
  this->read_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_STATUS, &status);
  return status & 0x01;
}

uint32_t TSL2591Component::get_combined_illuminance() {
  this->enable();
  // Wait x ms for ADC to complete and signal valid.
  // The max integration time is 600ms, so that's our max delay.
  // (But we use 620ms as a bit of slack.)
  // We'll do mini-delays and break out as soon as the ADC is good.
  bool avalid;
  const uint8_t mini_delay = 50;
  for (uint16_t d = 0; d < 620; d += mini_delay) {
    avalid = this->is_adc_valid();
    if (avalid) {
      break;
    }
    // we only log this if we need any delay, since normally we don't
    ESP_LOGD(TAG, "   after %3d ms: ADC valid? %s", d, avalid ? "true" : "false");
    delay(mini_delay);
  }
  if (!avalid) {
    // still not valid after a sutiable delay
    // we don't mark the device as failed since it might come around in the future (probably not :-()
    ESP_LOGE(TAG, "tsl2591 device '%s' did not return valid readings.", this->name_);
    this->disable_internal_();
    return 0;
  }

  // CHAN0 must be read before CHAN1
  // See: https://forums.adafruit.com/viewtopic.php?f=19&t=124176
  // Also, low byte must be read before high byte..
  // We read the registers in the order described in the datasheet.
  uint32_t x32;
  uint8_t ch0low, ch0high, ch1low, ch1high;
  uint16_t ch0_16;
  uint16_t ch1_16;
  this->read_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN0_LOW, &ch0low);
  this->read_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN0_HIGH, &ch0high);
  ch0_16 = (ch0high << 8) | ch0low;
  this->read_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN1_LOW, &ch1low);
  this->read_byte(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN1_HIGH, &ch1high);
  ch1_16 = (ch1high << 8) | ch1low;
  x32 = (ch1_16 << 16) | ch0_16;

  this->disable_internal_();
  return x32;
}
uint16_t TSL2591Component::get_illuminance(TSL2591SensorChannel channel) {
  uint32_t combined = this->get_combined_illuminance();
  return this->get_illuminance(channel, combined);
}
// logic cloned from Adafruit TSL2591 library
uint16_t TSL2591Component::get_illuminance(TSL2591SensorChannel channel, uint32_t combined_illuminance) {
  if (channel == TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM) {
    // Reads two byte value from channel 0 (visible + infrared)
    return (combined_illuminance & 0xFFFF);
  } else if (channel == TSL2591_SENSOR_CHANNEL_INFRARED) {
    // Reads two byte value from channel 1 (infrared)
    return (combined_illuminance >> 16);
  } else if (channel == TSL2591_SENSOR_CHANNEL_VISIBLE) {
    // Reads all and subtracts out the infrared
    return ((combined_illuminance & 0xFFFF) - (combined_illuminance >> 16));
  }
  // unknown channel!
  ESP_LOGE(TAG, "TSL2591Component::get_illuminance() caller requested an unknown channel: %d", channel);
  return 0;
}

/** Calculates a lux value from the two TSL2591 physical sensor ADC readings.
 *
 * The lux calculation is copied from the Adafruit TSL2591 library.
 * There is some debate about whether it is the correct lux equation to use.
 * We use that lux equation because (a) it helps with a transition from
 * using that Adafruit library to using this ESPHome integration, and (b) we
 * don't have a definitive better idea.
 *
 * Since the ADC readings are available, you can ignore this method and
 * implement your own lux equation.
 *
 * @param full_spectrum The ADC reading for TSL2591 channel 0.
 * @param infrared The ADC reading for TSL2591 channel 1.
 */
float TSL2591Component::get_calculated_lux(uint16_t full_spectrum, uint16_t infrared) {
  // Check for overflow conditions first
  uint16_t max_count = (this->integration_time_ == TSL2591_INTEGRATION_TIME_100MS ? 36863 : 65535);
  if ((full_spectrum == max_count) || (infrared == max_count)) {
    // Signal an overflow
    ESP_LOGW(TAG, "Apparent saturation on TSL2591 (%s). You could reduce the gain.", this->name_);
    return -1.0F;
  }

  if ((full_spectrum == 0) && (infrared == 0)) {
    // trivial conversion; avoids divide by 0
    ESP_LOGW(TAG, "Zero reading on both TSL2591 (%s) sensors. Is the device having a problem?", this->name_);
    return 0.0F;
  }

  float atime = 100.F + (this->integration_time_ * 100);

  float again;
  switch (this->gain_) {
    case TSL2591_GAIN_LOW:
      again = 1.0F;
      break;
    case TSL2591_GAIN_MED:
      again = 25.0F;
      break;
    case TSL2591_GAIN_HIGH:
      again = 400.0F;
      break;
    case TSL2591_GAIN_MAX:
      again = 9500.0F;
      break;
    default:
      again = 1.0F;
      break;
  }

    // This lux equation is copied from the Adafruit TSL2591 v1.4.0.
    // See: https://github.com/adafruit/Adafruit_TSL2591_Library/issues/14
    // and that library code.
    // They said:
    //   Note: This algorithm is based on preliminary coefficients
    //   provided by AMS and may need to be updated in the future
    // However, we use gain multipliers that are more in line with the midpoints
    // of ranges from the datasheet. We don't know why the other libraries
    // used the values they did for HIGH and MAX.
    // If cpl or full_spectrum are 0, this will return NaN due to divide by 0.
#define TSL2591_LUX_DGF (408.0F)  // Lux device factor * glass attenuation factor
  float cpl = (atime * again) / TSL2591_LUX_DGF;
  float lux = (((float) full_spectrum - (float) infrared)) * (1.0F - ((float) infrared / (float) full_spectrum)) / cpl;
  return lux;
}

}  // namespace tsl2591
}  // namespace esphome
