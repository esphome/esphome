// Implementation based on:
//  - Official Datasheet:
//    https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/EMC2301-2-3-5-Data-Sheet-DS20006532A.pdf
//  - RPM to TACH Counts Conversion: https://ww1.microchip.com/downloads/en/AppNotes/en562764.pdf
//  - EMC230x Arduino Library: https://github.com/earlynerd/emc230x_arduino

#include "emc230x.h"
#include "esphome/core/log.h"

namespace esphome::emc230x {

static const char *const TAG = "emc230x.component";

static const uint16_t EMC230X_TACH_FREQUENCY = 32768U;  // Frequency of the tachometer signal

static const uint8_t EMC2301_PRODUCT_ID = 0x37;  // EMC2301 Product ID
static const uint8_t EMC2302_PRODUCT_ID = 0x36;  // EMC2302 Product ID
static const uint8_t EMC2303_PRODUCT_ID = 0x35;  // EMC2303 Product ID
static const uint8_t EMC2305_PRODUCT_ID = 0x34;  // EMC2305 Product ID

// EMC230X registers from the datasheet. We only define what we use.
static const uint8_t EMC230X_REGISTER_CONFIG = 0x20;             // Configuration register
static const uint8_t EMC230X_REGISTER_PWM_OUTPUT_CONFIG = 0x2A;  // PWM output configuration register
static const uint8_t EMC230X_REGISTER_PWM_BASE_F45 = 0x2C;       // PWM 4/5 base frequency configuration register
static const uint8_t EMC230X_REGISTER_PWM_BASE_F123 = 0x2D;      // PWM 1/2/3 base frequency configuration register
static const uint8_t EMC230X_REGISTER_PRODUCT_ID = 0xFD;         // Product Identification register

// EMC230X fan register offsets from base to calculate specific fan registers
static const uint8_t EMC230X_REGISTER_FAN_DRIVE_SETTING = 0x00;      // Fan drive setting register
static const uint8_t EMC230X_REGISTER_FAN_PWM_DIVIDE = 0x01;         // Fan PWM frequency divide register
static const uint8_t EMC230X_REGISTER_FAN_CONFIG = 0x02;             // Fan configuration register
static const uint8_t EMC230X_REGISTER_FAN_CONFIG_2 = 0x03;           // Fan configuration 2 register
static const uint8_t EMC230X_REGISTER_FAN_SPIN = 0x06;               // Fan spin-up configuration register
static const uint8_t EMC230X_REGISTER_FAN_MAX_STEP = 0x07;           // Fan maximum step size register
static const uint8_t EMC230X_REGISTER_FAN_TACH_READING_HIGH = 0x0E;  // Fan tachometer reading high byte register
static const uint8_t EMC230X_REGISTER_FAN_TACH_READING_LOW = 0x0F;   // Fan tachometer reading low byte register

// EMC230X configuration bits from the datasheet. We only define what we use.

// Enables the Watchdog Timer to operate in Continuous Mode
// 1 = The Watchdog Timer operates continuously.
// 0 (default) = The Watchdog Timer does not operate continuously. It will function upon
// power-up and at no other time
static const uint8_t EMC230X_WD_EN = 1 << 5;

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

// Convert the update time enum to a human-readable string for logging
static const char *update_time_to_str(Emc230xUpdateTime update_time) {
  switch (update_time) {
    case EMC230X_UPDATE_TIME_100MS:
      return "100ms";
    case EMC230X_UPDATE_TIME_200MS:
      return "200ms";
    case EMC230X_UPDATE_TIME_300MS:
      return "300ms";
    case EMC230X_UPDATE_TIME_400MS:
      return "400ms";
    case EMC230X_UPDATE_TIME_500MS:
      return "500ms";
    case EMC230X_UPDATE_TIME_800MS:
      return "800ms";
    case EMC230X_UPDATE_TIME_1200MS:
      return "1200ms";
    case EMC230X_UPDATE_TIME_1600MS:
      return "1600ms";
    default:
      return "UNKNOWN";
  }
}

// Convert the spin-up level enum to a human-readable string for logging
static const char *spin_up_level_to_str(Emc230xSpinUpLevel spin_up_level) {
  switch (spin_up_level) {
    case EMC230X_SPIN_UP_LEVEL_30:
      return "30%";
    case EMC230X_SPIN_UP_LEVEL_35:
      return "35%";
    case EMC230X_SPIN_UP_LEVEL_40:
      return "40%";
    case EMC230X_SPIN_UP_LEVEL_45:
      return "45%";
    case EMC230X_SPIN_UP_LEVEL_50:
      return "50%";
    case EMC230X_SPIN_UP_LEVEL_55:
      return "55%";
    case EMC230X_SPIN_UP_LEVEL_60:
      return "60%";
    case EMC230X_SPIN_UP_LEVEL_65:
      return "65%";
    default:
      return "UNKNOWN";
  }
}

// Convert the spin-up time enum to a human-readable string for logging
static const char *spin_up_time_to_str(Emc230xSpinUpTime spin_up_time) {
  switch (spin_up_time) {
    case EMC230X_SPIN_UP_TIME_250MS:
      return "250ms";
    case EMC230X_SPIN_UP_TIME_500MS:
      return "500ms";
    case EMC230X_SPIN_UP_TIME_1S:
      return "1s";
    case EMC230X_SPIN_UP_TIME_2S:
      return "2s";
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
    this->mark_failed(LOG_STR("Failed to read Product ID"));
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
      this->mark_failed(LOG_STR("Unknown Product ID"));
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

  // Set the PWM frequency and output type (push-pull or open-drain) for each fan
  uint8_t pwm_output_config = 0;
  uint8_t pwm_base_f45 = 0;
  uint8_t pwm_base_f123 = 0;
  for (size_t i = 0; i < this->fan_count_; i++) {
    // Set the PWM output type for this fan (bit 0-4 of the PWM output configuration register)
    pwm_output_config |= (this->pwm_push_pull_[i] << i);

    // Set the PWM frequency for this fan in the appropriate base register
    if (i > 2) {
      pwm_base_f45 |= (this->pwm_frequencies_[i] << ((i - 3) * 2));
    } else {
      pwm_base_f123 |= (this->pwm_frequencies_[i] << (i * 2));
    }
  }
  // Write the PWM output configuration
  if (!this->write_byte(EMC230X_REGISTER_PWM_OUTPUT_CONFIG, pwm_output_config)) {
    ESP_LOGE(TAG, "Failed to write PWM output configuration");
    this->mark_failed(LOG_STR("Failed to write PWM output configuration"));
    return;
  }
  // Write the PWM frequency base registers
  if (!this->write_byte(EMC230X_REGISTER_PWM_BASE_F123, pwm_base_f123) ||
      !this->write_byte(EMC230X_REGISTER_PWM_BASE_F45, pwm_base_f45)) {
    ESP_LOGE(TAG, "Failed to write PWM frequency configuration");
    this->mark_failed(LOG_STR("Failed to write PWM frequency configuration"));
    return;
  }

  // Configure each fan
  for (size_t i = 0; i < this->fan_count_; i++) {
    // Set the PWM divider for this fan
    if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[i] + EMC230X_REGISTER_FAN_PWM_DIVIDE, this->pwm_dividers_[i])) {
      ESP_LOGE(TAG, "Failed to write PWM divider for fan %d", i + 1);
      this->mark_failed(LOG_STR("Failed to write PWM divider"));
      return;
    }

    // Configure the spin-up settings for this fan
    uint8_t spin_config = 0;

    // Enable spin-up 100% drive kick if configured (bit 5)
    if (!this->spin_up_kick_[i]) {
      spin_config |= 1 << 5;
    }

    // Set the spin-up level for this fan (bits 4-2)
    spin_config |= (this->spin_up_levels_[i] << 2);

    // Set the spin-up time for this fan (bits 1-0)
    spin_config |= this->spin_up_times_[i];

    // Write the spin configuration to the appropriate register
    if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[i] + EMC230X_REGISTER_FAN_SPIN, spin_config)) {
      ESP_LOGE(TAG, "Failed to write spin configuration for fan %d", i + 1);
      this->mark_failed(LOG_STR("Failed to write spin configuration"));
      return;
    }

    uint8_t fan_config_2 = 0;

    // Enable Ramp Rate Control for this fan if the max step size is configured to a non-zero value (bit 6)
    if (this->max_step_sizes_[i] != 0) {
      fan_config_2 |= 1 << 6;
    }

    // Enable noise filter for this fan if configured (bit 5)
    if (this->noise_filters_[i]) {
      fan_config_2 |= 1 << 5;
    }

    // Write the fan configuration 2 to the appropriate register
    if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[i] + EMC230X_REGISTER_FAN_CONFIG_2, fan_config_2)) {
      ESP_LOGE(TAG, "Failed to write configuration 2 for fan %d", i + 1);
      this->mark_failed(LOG_STR("Failed to write configuration 2"));
      return;
    }

    // Set the fan maximum step size
    if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[i] + EMC230X_REGISTER_FAN_MAX_STEP, this->max_step_sizes_[i])) {
      ESP_LOGE(TAG, "Failed to write max step size for fan %d", i + 1);
      this->mark_failed(LOG_STR("Failed to write max step size"));
      return;
    }

    uint8_t fan_config = 0;

    // Set the minimum fan speed measured and reported (RANGE) (bits 6-5)
    fan_config |= (this->min_speed_measurements_[i] << 5);

    // Set the number of pulses per revolution for this fan (bits 4-3)
    fan_config |= ((this->pulses_per_revolution_[i] - 1) << 3);

    // Set the update rate for this fan (bits 2-0)
    fan_config |= this->update_times_[i];

    // Write the fan configuration to the appropriate register
    if (!this->write_byte(EMC230X_FAN_REGISTER_BASES[i] + EMC230X_REGISTER_FAN_CONFIG, fan_config)) {
      ESP_LOGE(TAG, "Failed to write configuration for fan %d", i + 1);
      this->mark_failed(LOG_STR("Failed to write configuration"));
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
    // Original formula from datasheet:
    // RPM = (1/pulses_per_revolution)*((edge_count-1)/(tach_count*(1/multiplier)))*tachometer_frequency*60
    // Simplified to:
    // RPM = (60/pulses_per_revolution)*tachometer_frequency*(edge_count-1)*multiplier/tach_count
    // We pre-calculate everything except the tachometer count since that is what we read from the sensor.
    // Because pulses_per_revolution can only be 1 - 4, the division is exact and we can use integer math.
    this->rpm_conversion_constants_[i] =
        (60UL / this->pulses_per_revolution_[i]) * EMC230X_TACH_FREQUENCY * (edge_count - 1UL) * multiplier;
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
    ESP_LOGCONFIG(TAG, "    PWM Output Type: %s", this->pwm_push_pull_[i] ? "Push-Pull" : "Open-Drain");
    ESP_LOGCONFIG(TAG, "    PWM Frequency: %s", pwm_frequency_to_str(this->pwm_frequencies_[i]));
    ESP_LOGCONFIG(TAG, "    PWM Divider: %d", this->pwm_dividers_[i]);
    ESP_LOGCONFIG(TAG, "    Pulses per Revolution: %d", this->pulses_per_revolution_[i]);
    ESP_LOGCONFIG(TAG, "    Minimum speed Measurement: %s",
                  min_speed_measurement_to_str(this->min_speed_measurements_[i]));
    ESP_LOGCONFIG(TAG, "    RPM Conversion Constant: %u", this->rpm_conversion_constants_[i]);
    ESP_LOGCONFIG(TAG, "    Update Time: %s", update_time_to_str(this->update_times_[i]));
    ESP_LOGCONFIG(TAG, "    Max Step Size: %d", this->max_step_sizes_[i]);
    ESP_LOGCONFIG(TAG, "    Noise Filter: %s", YESNO(this->noise_filters_[i]));
    ESP_LOGCONFIG(TAG, "    Spin-up Kick: %s", YESNO(this->spin_up_kick_[i]));
    ESP_LOGCONFIG(TAG, "    Spin-up Level: %s", spin_up_level_to_str(this->spin_up_levels_[i]));
    ESP_LOGCONFIG(TAG, "    Spin-up Time: %s", spin_up_time_to_str(this->spin_up_times_[i]));
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
  return static_cast<float>(this->rpm_conversion_constants_[fan - 1]) / tach_count;
}

}  // namespace esphome::emc230x
