// Implementation based on:
//  - Official Datasheet:
//    https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/EMC2301-2-3-5-Data-Sheet-DS20006532A.pdf
//  - RPM to TACH Counts Conversion: https://ww1.microchip.com/downloads/en/AppNotes/en562764.pdf
//  - EMC230x Arduino Library: https://github.com/earlynerd/emc230x_arduino

#include "emc2303.h"
#include "esphome/core/log.h"

namespace esphome::emc2303 {

static const char *const TAG = "emc2303.component";

static const float EMC2303_TACH_FREQUENCY = 32768.0f;  // Frequency of the tachometer signal

static const uint8_t EMC2303_PRODUCT_ID = 0x35;  // EMC2303 default device id from part id

// EMC2303 registers from the datasheet. We only define what we use.
static const uint8_t EMC2303_REGISTER_CONFIG = 0x20;         // Configuration register
static const uint8_t EMC2303_REGISTER_PRODUCT_ID = 0xFD;     // Product Identification register
static const uint8_t EMC2303_REGISTER_PWM_BASE_F123 = 0x2D;  // PWM 1/2/3 base frequency configuration register

// EMC2303 fan register offsets from base to calculate specific fan registers
static const uint8_t EMC2303_REGISTER_FAN_DRIVE_SETTING = 0x00;      // Fan drive setting register
static const uint8_t EMC2303_REGISTER_FAN_CONFIG = 0x02;             // Fan configuration register
static const uint8_t EMC2303_REGISTER_FAN_TACH_READING_HIGH = 0x0E;  // Fan tachometer reading high byte register
static const uint8_t EMC2303_REGISTER_FAN_TACH_READING_LOW = 0x0F;   // Fan tachometer reading low byte register

// EMC2303 configuration bits from the datasheet. We only define what we use.

// Enables the Watchdog Timer to operate in Continuous Mode
// 1 = The Watchdog Timer operates continuously.
// 0 (default) = The Watchdog Timer does not operate continuously. It will function upon
// power-up and at no other time
static const uint8_t EMC2303_WD_EN = 1 << 5;

// Enable Closed Loop algorithm for the fan
// 1 = Closed Loop algorithm is enabled for the fan. Changes to the fan drive setting will be ignored.
// 0 (default) = Closed Loop algorithm is disabled for the fan and the fan is placed in Direct Setting
// mode. Changes to the fan drive setting will change the PWM duty cycle applied to the fan.
static const uint8_t EMC2303_ENAG = 1 << 7;

// Lookup table for the base registers of each fan, used to calculate specific register addresses for each fan
static const uint8_t EMC2303_FAN_REGISTER_BASES[3] = {
    0x30,  // Fan 1 base register address
    0x40,  // Fan 2 base register address
    0x50   // Fan 3 base register address
};

// Convert the PWM frequency enum to a human-readable string for logging
static const char *pwm_frequency_to_str(Emc2303PwmFrequency frequency) {
  switch (frequency) {
    case EMC2303_PWM_FREQUENCY_26000HZ:
      return "26kHz";
    case EMC2303_PWM_FREQUENCY_19531HZ:
      return "19.5kHz";
    case EMC2303_PWM_FREQUENCY_4882HZ:
      return "4.9kHz";
    case EMC2303_PWM_FREQUENCY_2441HZ:
      return "2.4kHz";
    default:
      return "UNKNOWN";
  }
}

// Convert the minimum speed measurement enum to a human-readable string for logging
static const char *min_speed_measurement_to_str(Emc2303MinSpeedMeasurement measurement) {
  switch (measurement) {
    case EMC2303_MIN_SPEED_500RPM:
      return "500RPM";
    case EMC2303_MIN_SPEED_1000RPM:
      return "1000RPM";
    case EMC2303_MIN_SPEED_2000RPM:
      return "2000RPM";
    case EMC2303_MIN_SPEED_4000RPM:
      return "4000RPM";
    default:
      return "UNKNOWN";
  }
}

float Emc2303Component::get_setup_priority() const { return setup_priority::HARDWARE; }

void Emc2303Component::setup() {
  // make sure we're talking to the right chip
  uint8_t product_id = reg(EMC2303_REGISTER_PRODUCT_ID).get();
  if (product_id != EMC2303_PRODUCT_ID) {
    ESP_LOGE(TAG, "Wrong Product ID %02X", product_id);
    this->mark_failed();
    return;
  }

  // Configure EMC2303
  i2c::I2CRegister config = reg(EMC2303_REGISTER_CONFIG);

  // Enable or disable the watchdog time
  if (this->watchdog_) {
    config |= EMC2303_WD_EN;
  } else {
    config &= ~EMC2303_WD_EN;
  }

  // Configure each fan
  // Start with a clean configuration for all fans
  uint8_t pwm_base_f123 = 0;
  for (size_t i = 0; i < 3; i++) {
    // Set the PWM frequency for this fan
    pwm_base_f123 |= (this->pwm_frequencies_[i] << (i * 2));

    // Start with a clean configuration for the fan
    uint8_t fan_config = 0;

    // Disable Closed Loop algorithm and place the fan in Direct Setting mode (bit 7)
    // Commenting out because the default is already 0
    // fan_config &= 0xFF ^ EMC2303_ENAG;

    // Set the minimum fan speed measured and reported (RANGE) (bits 7:5)
    fan_config |= (this->min_speed_measurements_[i] << 5);

    // Set the number of pulses per revolution for this fan (bits 5:3)
    fan_config |= ((this->pulses_per_revolution_[i] - 1) << 3);

    // Write the fan configuration to the appropriate register
    if (!this->write_byte(EMC2303_FAN_REGISTER_BASES[i] + EMC2303_REGISTER_FAN_CONFIG, fan_config)) {
      ESP_LOGE(TAG, "Failed to write configuration for fan %d", i + 1);
      this->mark_failed();
      return;
    }

    // Calculate actual edges measured based on the number of pulses per revolution
    // 00 = 3 edges (1 pulse per revolution), 01 = 5 edges (2 pulses per revolution)
    // 10 = 7 edges (3 pulses per revolution), 11 = 9 edges (4 pulses per revolution)
    const uint8_t edge_count = 2 * this->pulses_per_revolution_[i] + 1;

    // Calculate the multiplier based on the RANGE bit
    // 00 = 1x (500 RPM min), 01 = 2x (1000 RPM min)
    // 10 = 4x (2000 RPM min), 11 = 8x (4000 RPM min)
    const uint8_t multiplier = 1 << this->min_speed_measurements_[i];

    // Calculate the conversion constant from tachometer reading to RPM
    // RPM = (1 / pulses_per_revolution) * tachometer_frequency * 60 * (edge_count - 1) * multiplier / tach_count
    this->rpm_conversion_constants_[i] =
        (1.0f / this->pulses_per_revolution_[i]) * EMC2303_TACH_FREQUENCY * 60.0f * (edge_count - 1) * multiplier;
  }

  // Write the PWM frequency configuration for all fans to the appropriate register
  if (!this->write_byte(EMC2303_REGISTER_PWM_BASE_F123, pwm_base_f123)) {
    ESP_LOGE(TAG, "Failed to write PWM frequency configuration");
    this->mark_failed();
    return;
  }
}

void Emc2303Component::dump_config() {
  ESP_LOGCONFIG(TAG, "EMC2303");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Watchdog: %s", YESNO(this->watchdog_));
  for (size_t i = 0; i < 3; i++) {
    ESP_LOGCONFIG(TAG, "  Fan %d:", i + 1);
    ESP_LOGCONFIG(TAG, "    PWM Frequency: %s", pwm_frequency_to_str(this->pwm_frequencies_[i]));
    ESP_LOGCONFIG(TAG, "    Pulses per Revolution: %d", this->pulses_per_revolution_[i]);
    ESP_LOGCONFIG(TAG, "    Minimum speed Measurement: %s",
                  min_speed_measurement_to_str(this->min_speed_measurements_[i]));
  }
}

void Emc2303Component::set_duty_cycle(uint8_t fan, float value) {
  uint8_t duty_cycle = remap(value, 0.0f, 1.0f, (uint8_t) 0, (uint8_t) 255);
  ESP_LOGD(TAG, "Setting fan %d duty cycle to %d (%.1f%%)", fan, duty_cycle, value * 100.0f);
  if (!this->write_byte(EMC2303_FAN_REGISTER_BASES[fan - 1] + EMC2303_REGISTER_FAN_DRIVE_SETTING, duty_cycle)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning();
    return;
  }
}

float Emc2303Component::get_speed(uint8_t fan) {
  const uint8_t fan_base = EMC2303_FAN_REGISTER_BASES[fan - 1];
  uint8_t tach_high, tach_low;
  if (!this->read_byte(fan_base + EMC2303_REGISTER_FAN_TACH_READING_HIGH, &tach_high) ||
      !this->read_byte(fan_base + EMC2303_REGISTER_FAN_TACH_READING_LOW, &tach_low)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning();
    return NAN;
  }

  uint16_t tach_count = ((uint16_t) tach_high << 5) | ((tach_low >> 3) & 0x1F);

  // Edge cases
  if (tach_count == 0 || tach_high == 0xFF) {
    return 0.0f;
  }

  // Calculate RPM and return
  return this->rpm_conversion_constants_[fan - 1] / tach_count;
}

}  // namespace esphome::emc2303
