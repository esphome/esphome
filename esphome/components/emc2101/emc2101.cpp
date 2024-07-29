// Implementation based on:
//  - Adafruit_EMC2101: https://github.com/adafruit/Adafruit_EMC2101
//  - Official Datasheet: https://ww1.microchip.com/downloads/en/DeviceDoc/2101.pdf

#include "esphome/core/log.h"
#include "emc2101.h"

namespace esphome {
namespace emc2101 {

static const char *const TAG = "EMC2101";

static const uint8_t EMC2101_CHIP_ID = 0x16;      // EMC2101 default device id from part id
static const uint8_t EMC2101_ALT_CHIP_ID = 0x28;  // EMC2101 alternate device id from part id

// EMC2101 registers from the datasheet. We only define what we use.
static const uint8_t EMC2101_REGISTER_INTERNAL_TEMP = 0x00;      // The internal temperature register
static const uint8_t EMC2101_REGISTER_EXTERNAL_TEMP_MSB = 0x01;  // high byte for the external temperature reading
static const uint8_t EMC2101_REGISTER_DAC_CONV_RATE = 0x04;      // DAC convesion rate config
static const uint8_t EMC2101_REGISTER_EXTERNAL_TEMP_LSB = 0x10;  // low byte for the external temperature reading
static const uint8_t EMC2101_REGISTER_CONFIG = 0x03;             // configuration register
static const uint8_t EMC2101_REGISTER_TACH_LSB = 0x46;           // Tach RPM data low byte
static const uint8_t EMC2101_REGISTER_TACH_MSB = 0x47;           // Tach RPM data high byte
static const uint8_t EMC2101_REGISTER_FAN_CONFIG = 0x4A;         // General fan config register
static const uint8_t EMC2101_REGISTER_FAN_SETTING = 0x4C;        // Fan speed for non-LUT settings
static const uint8_t EMC2101_REGISTER_PWM_FREQ = 0x4D;           // PWM frequency setting
static const uint8_t EMC2101_REGISTER_PWM_DIV = 0x4E;            // PWM frequency divisor
static const uint8_t EMC2101_REGISTER_WHOAMI = 0xFD;             // Chip ID register

// EMC2101 configuration bits from the datasheet. We only define what we use.

// Determines the funcionallity of the ALERT/TACH pin.
// 0 (default): The ALERT/TECH pin will function as an open drain, active low interrupt.
// 1: The ALERT/TECH pin will function as a high impedance TACH input. This may require an
// external pull-up resistor to set the proper signaling levels.
static const uint8_t EMC2101_ALT_TCH_BIT = 1 << 2;

// Determines the FAN output mode.
// 0 (default): PWM output enabled at FAN pin.
// 1: DAC output enabled at FAN ping.
static const uint8_t EMC2101_DAC_BIT = 1 << 4;

// Overrides the CLK_SEL bit and uses the Frequency Divide Register to determine
// the base PWM frequency. It is recommended that this bit be set for maximum PWM resolution.
// 0 (default): The base clock frequency for the PWM is determined by the CLK_SEL bit.
// 1 (recommended): The base clock that is used to determine the PWM frequency is set by the
// Frequency Divide Register
static const uint8_t EMC2101_CLK_OVR_BIT = 1 << 2;

// Sets the polarity of the Fan output driver.
// 0 (default): The polarity of the Fan output driver is non-inverted. A '00h' setting will
// correspond to a 0% duty cycle or a minimum DAC output voltage.
// 1: The polarity of the Fan output driver is inverted. A '00h' setting will correspond to a
// 100% duty cycle or a maximum DAC output voltage.
static const uint8_t EMC2101_POLARITY_BIT = 1 << 4;

float Emc2101Component::get_setup_priority() const { return setup_priority::HARDWARE; }

void Emc2101Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Emc2101 sensor...");

  // make sure we're talking to the right chip
  uint8_t chip_id = reg(EMC2101_REGISTER_WHOAMI).get();
  if ((chip_id != EMC2101_CHIP_ID) && (chip_id != EMC2101_ALT_CHIP_ID)) {
    ESP_LOGE(TAG, "Wrong chip ID %02X", chip_id);
    this->mark_failed();
    return;
  }

  // Configure EMC2101
  i2c::I2CRegister config = reg(EMC2101_REGISTER_CONFIG);
  config |= EMC2101_ALT_TCH_BIT;
  if (this->dac_mode_) {
    config |= EMC2101_DAC_BIT;
  }
  if (this->inverted_) {
    config |= EMC2101_POLARITY_BIT;
  }

  if (this->dac_mode_) {  // DAC mode configurations
    // set DAC conversion rate
    reg(EMC2101_REGISTER_DAC_CONV_RATE) = this->dac_conversion_rate_;
  } else {  // PWM mode configurations
    // set PWM divider
    reg(EMC2101_REGISTER_FAN_CONFIG) |= EMC2101_CLK_OVR_BIT;
    reg(EMC2101_REGISTER_PWM_DIV) = this->pwm_divider_;

    // set PWM resolution
    reg(EMC2101_REGISTER_PWM_FREQ) = this->pwm_resolution_;
  }
}

void Emc2101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Emc2101 component:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with EMC2101 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->dac_mode_ ? "DAC" : "PWM");
  if (this->dac_mode_) {
    ESP_LOGCONFIG(TAG, "  DAC Conversion Rate: %X", this->dac_conversion_rate_);
  } else {
    ESP_LOGCONFIG(TAG, "  PWM Resolution: %02X", this->pwm_resolution_);
    ESP_LOGCONFIG(TAG, "  PWM Divider: %02X", this->pwm_divider_);
  }
  ESP_LOGCONFIG(TAG, "  Inverted: %s", YESNO(this->inverted_));
}

void Emc2101Component::set_duty_cycle(float value) {
  uint8_t duty_cycle = remap(value, 0.0f, 1.0f, (uint8_t) 0, this->max_output_value_);
  ESP_LOGD(TAG, "Setting duty fan setting to %02X", duty_cycle);
  if (!this->write_byte(EMC2101_REGISTER_FAN_SETTING, duty_cycle)) {
    ESP_LOGE(TAG, "Communication with EMC2101 failed!");
    this->status_set_warning();
    return;
  }
}

float Emc2101Component::get_duty_cycle() {
  uint8_t duty_cycle;
  if (!this->read_byte(EMC2101_REGISTER_FAN_SETTING, &duty_cycle)) {
    ESP_LOGE(TAG, "Communication with EMC2101 failed!");
    this->status_set_warning();
    return NAN;
  }
  return remap(duty_cycle, (uint8_t) 0, this->max_output_value_, 0.0f, 1.0f);
}

float Emc2101Component::get_internal_temperature() {
  uint8_t temperature;
  if (!this->read_byte(EMC2101_REGISTER_INTERNAL_TEMP, &temperature)) {
    ESP_LOGE(TAG, "Communication with EMC2101 failed!");
    this->status_set_warning();
    return NAN;
  }
  return temperature;
}

float Emc2101Component::get_external_temperature() {
  // Read **MSB** first to match 'Data Read Interlock' behavior from 6.1 of datasheet
  uint8_t lsb, msb;
  if (!this->read_byte(EMC2101_REGISTER_EXTERNAL_TEMP_MSB, &msb) ||
      !this->read_byte(EMC2101_REGISTER_EXTERNAL_TEMP_LSB, &lsb)) {
    ESP_LOGE(TAG, "Communication with EMC2101 failed!");
    this->status_set_warning();
    return NAN;
  }

  // join msb and lsb (5 least significant bits are not used)
  uint16_t raw = (msb << 8 | lsb) >> 5;
  return raw * 0.125;
}

float Emc2101Component::get_speed() {
  // Read **LSB** first to match 'Data Read Interlock' behavior from 6.1 of datasheet
  uint8_t lsb, msb;
  if (!this->read_byte(EMC2101_REGISTER_TACH_LSB, &lsb) || !this->read_byte(EMC2101_REGISTER_TACH_MSB, &msb)) {
    ESP_LOGE(TAG, "Communication with EMC2101 failed!");
    this->status_set_warning();
    return NAN;
  }

  // calculate RPMs
  uint16_t tach = msb << 8 | lsb;
  return tach == 0xFFFF ? 0.0f : 5400000.0f / tach;
}

}  // namespace emc2101
}  // namespace esphome
