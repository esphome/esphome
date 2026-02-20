// Implementation based on:
//  - Official Datasheet:
//    https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/EMC2301-2-3-5-Data-Sheet-DS20006532A.pdf
//  - RPM to TACH Counts Conversion: https://ww1.microchip.com/downloads/en/AppNotes/en562764.pdf
//  - EMC230x Arduino Library: https://github.com/earlynerd/emc230x_arduino

#include "emc230x.h"
#include "esphome/core/log.h"

namespace esphome::emc230x {

static const char *const TAG = "emc230x.component";

static const float EMC230X_TACH_FREQUENCY = 32768.0f;  // Frequency of the tachometer signal

static const uint8_t EMC2301_PRODUCT_ID = 0x37;  // EMC2301 Product ID
static const uint8_t EMC2302_PRODUCT_ID = 0x36;  // EMC2302 Product ID
static const uint8_t EMC2303_PRODUCT_ID = 0x35;  // EMC2303 Product ID
static const uint8_t EMC2305_PRODUCT_ID = 0x34;  // EMC2305 Product ID

// EMC230X registers from the datasheet. We only define what we use.
static const uint8_t EMC230X_REGISTER_CONFIG = 0x20;         // Configuration register
static const uint8_t EMC230X_REGISTER_PRODUCT_ID = 0xFD;     // Product Identification register
static const uint8_t EMC230X_REGISTER_PWM_BASE_F45 = 0x2C;   // PWM 4/5 base frequency configuration register
static const uint8_t EMC230X_REGISTER_PWM_BASE_F123 = 0x2D;  // PWM 1/2/3 base frequency configuration register

// EMC230X fan register offsets from base to calculate specific fan registers
static const uint8_t EMC230X_REGISTER_FAN_DRIVE_SETTING = 0x00;      // Fan drive setting register
static const uint8_t EMC230X_REGISTER_FAN_CONFIG = 0x02;             // Fan configuration register
static const uint8_t EMC230X_REGISTER_FAN_TACH_READING_HIGH = 0x0E;  // Fan tachometer reading high byte register
static const uint8_t EMC230X_REGISTER_FAN_TACH_READING_LOW = 0x0F;   // Fan tachometer reading low byte register

// EMC230X configuration bits from the datasheet. We only define what we use.

// Enables the Watchdog Timer to operate in Continuous Mode
// 1 = The Watchdog Timer operates continuously.
// 0 (default) = The Watchdog Timer does not operate continuously. It will function upon
// power-up and at no other time
static const uint8_t EMC230X_WD_EN = 1 << 5;

// Enable Closed Loop algorithm for the fan
// 1 = Closed Loop algorithm is enabled for the fan. Changes to the fan drive setting will be ignored.
// 0 (default) = Closed Loop algorithm is disabled for the fan and the fan is placed in Direct Setting
// mode. Changes to the fan drive setting will change the PWM duty cycle applied to the fan.
static const uint8_t EMC230X_ENAG = 1 << 7;

// Lookup table for the base registers of each fan, used to calculate specific register addresses for each fan
static const uint8_t EMC230X_FAN_REGISTER_BASES[5] = {
    0x30,  // Fan 1 base register address
    0x40,  // Fan 2 base register address
    0x50,  // Fan 3 base register address
    0x60,  // Fan 4 base register address
    0x70   // Fan 5 base register address
};

// Convert the EMC230X model enum to a human-readable string for logging
static const char *emc230x_model_to_str(Emc230xModel model) {
  switch (model) {
    case EMC2301:
      return "EMC2301";
    case EMC2302:
      return "EMC2302";
    case EMC2303:
      return "EMC2303";
    case EMC2305:
      return "EMC2305";
    default:
      return "UNKNOWN";
  }
}

// Convert the PWM frequency enum to a human-readable string for logging
static const char *pwm_frequency_to_str(Emc230xPwmFrequency frequency) {
  switch (frequency) {
    case EMC230X_PWM_FREQUENCY_26000HZ:
      return "26kHz";
    case EMC230X_PWM_FREQUENCY_19531HZ:
      return "19.5kHz";
    case EMC230X_PWM_FREQUENCY_4882HZ:
      return "4.9kHz";
    case EMC230X_PWM_FREQUENCY_2441HZ:
      return "2.4kHz";
    default:
      return "UNKNOWN";
  }
}

// Convert the minimum speed measurement enum to a human-readable string for logging
static const char *min_speed_measurement_to_str(Emc230xMinSpeedMeasurement measurement) {
  switch (measurement) {
    case EMC230X_MIN_SPEED_500RPM:
      return "500RPM";
    case EMC230X_MIN_SPEED_1000RPM:
      return "1000RPM";
    case EMC230X_MIN_SPEED_2000RPM:
      return "2000RPM";
    case EMC230X_MIN_SPEED_4000RPM:
      return "4000RPM";
    default:
      return "UNKNOWN";
  }
}

float Emc230xComponent::get_setup_priority() const { return setup_priority::HARDWARE; }

void Emc230xComponent::setup() {
  // Identify the specific EMC230X model type by reading the Product ID register
  uint8_t product_id;
  if (!this->read_byte(EMC230X_REGISTER_PRODUCT_ID, &product_id)) {
    ESP_LOGE(TAG, "Failed to read Product ID");
    this->mark_failed();
    return;
  }
  switch (product_id) {
    case EMC2301_PRODUCT_ID:
      this->emc230x_model_ = EMC2301;
      this->fan_count_ = 1;
      break;
    case EMC2302_PRODUCT_ID:
      this->emc230x_model_ = EMC2302;
      this->fan_count_ = 2;
      break;
    case EMC2303_PRODUCT_ID:
      this->emc230x_model_ = EMC2303;
      this->fan_count_ = 3;
      break;
    case EMC2305_PRODUCT_ID:
      this->emc230x_model_ = EMC2305;
      this->fan_count_ = 5;
      break;
    default:
      ESP_LOGE(TAG, "Unknown Product ID: 0x%02X", product_id);
      this->mark_failed();
      return;
  }

  // Configure EMC230X
  i2c::I2CRegister config = reg(EMC230X_REGISTER_CONFIG);

  // Enable or disable the watchdog time
  if (this->watchdog_) {
    config |= EMC230X_WD_EN;
  } else {
    config &= ~EMC230X_WD_EN;
  }

  // Configure each fan
  // Start with a clean configuration for all fans
  uint8_t pwm_base_f45 = 0;
  uint8_t pwm_base_f123 = 0;
  for (size_t i = 0; i < this->fan_count_; i++) {
    // Set the PWM frequency for this fan
    if (i > 2) {
      pwm_base_f45 |= (this->pwm_frequencies_[i] << ((i - 3) * 2));
    } else {
      pwm_base_f123 |= (this->pwm_frequencies_[i] << (i * 2));
    }

    // Start with a clean configuration for the fan
    uint8_t fan_config = 0;

    // Disable Closed Loop algorithm and place the fan in Direct Setting mode (bit 7)
    // Commenting out because the default is already 0
    // fan_config &= 0xFF ^ EMC230X_ENAG;

    // Set the minimum fan speed measured and reported (RANGE) (bits 7:5)
    fan_config |= (this->min_speed_measurements_[i] << 5);

    // Set the number of pulses per revolution for this fan (bits 5:3)
    fan_config |= ((this->pulses_per_revolution_[i] - 1) << 3);

    // Write the fan configuration to the appropriate register
    if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[i] + EMC230X_REGISTER_FAN_CONFIG, fan_config)) {
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
        (1.0f / this->pulses_per_revolution_[i]) * EMC230X_TACH_FREQUENCY * 60.0f * (edge_count - 1) * multiplier;
  }

  // Write the PWM frequency configuration for all fans to the appropriate register
  if (!this->write_byte(EMC230X_REGISTER_PWM_BASE_F123, pwm_base_f123) ||
      !this->write_byte(EMC230X_REGISTER_PWM_BASE_F45, pwm_base_f45)) {
    ESP_LOGE(TAG, "Failed to write PWM frequency configuration");
    this->mark_failed();
    return;
  }
}

void Emc230xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EMC230X:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Model: %s", emc230x_model_to_str(this->emc230x_model_));
  ESP_LOGCONFIG(TAG, "  Watchdog: %s", YESNO(this->watchdog_));
  for (size_t i = 0; i < this->fan_count_; i++) {
    ESP_LOGCONFIG(TAG, "  Fan %d:", i + 1);
    ESP_LOGCONFIG(TAG, "    PWM Frequency: %s", pwm_frequency_to_str(this->pwm_frequencies_[i]));
    ESP_LOGCONFIG(TAG, "    Pulses per Revolution: %d", this->pulses_per_revolution_[i]);
    ESP_LOGCONFIG(TAG, "    Minimum speed Measurement: %s",
                  min_speed_measurement_to_str(this->min_speed_measurements_[i]));
    ESP_LOGCONFIG(TAG, "    RPM Conversion Constant: %.2f", this->rpm_conversion_constants_[i]);
  }
}

void Emc230xComponent::set_duty_cycle(uint8_t fan, float value) {
  if (fan > this->fan_count_) {
    ESP_LOGE(TAG, "Invalid fan number %d. This model only supports %d fans.", fan, this->fan_count_);
    return;
  }

  uint8_t duty_cycle = remap(value, 0.0f, 1.0f, (uint8_t) 0, (uint8_t) 255);
  ESP_LOGD(TAG, "Setting fan %d duty cycle to %d (%.1f%%)", fan, duty_cycle, value * 100.0f);
  if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[fan - 1] + EMC230X_REGISTER_FAN_DRIVE_SETTING, duty_cycle)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning();
    return;
  }
}

float Emc230xComponent::get_speed(uint8_t fan) {
  if (fan > this->fan_count_) {
    ESP_LOGE(TAG, "Invalid fan number %d. This model only supports %d fans.", fan, this->fan_count_);
    return NAN;
  }

  const uint8_t fan_base = EMC230X_FAN_REGISTER_BASES[fan - 1];
  uint8_t tach_high, tach_low;
  if (!this->read_byte(fan_base + EMC230X_REGISTER_FAN_TACH_READING_HIGH, &tach_high) ||
      !this->read_byte(fan_base + EMC230X_REGISTER_FAN_TACH_READING_LOW, &tach_low)) {
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

}  // namespace esphome::emc230x
