#include "at581x.h"
#include "esphome/core/log.h"

/* Select gain for AT581X (3dB per step for level1, 6dB per step for level 2), high value = small gain. (p12) */
const uint8_t GAIN_ADDR_TABLE[] = {0x5c, 0x63};
const uint8_t GAIN5C_TABLE[] = {0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78, 0x88, 0x98, 0xa8, 0xb8, 0xc8};
const uint8_t GAIN63_TABLE[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
const uint8_t GAIN61_VALUE = 0xCA;  // 0xC0 | 0x02 (freq present) | 0x08 (gain present)

/*!< Power consumption configuration table (p12). */
const uint8_t POWER_TABLE[] = {48, 56, 63, 70, 77, 91, 105, 115, 40, 44, 47, 51, 54, 61, 68, 78};
const uint8_t POWER67_TABLE[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
const uint8_t POWER68_TABLE[] = {0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
                                 24,  24,  24,  24,  24,  24,  24,  24};  // See Page 12, shift by 3 bits

/*!< Frequency Configuration table (p14/15 of datasheet). */
const uint8_t FREQ_ADDR = 0x61;
const uint16_t FREQ_TABLE[] = {5696, 5715, 5730, 5748, 5765, 5784, 5800, 5819, 5836, 5851, 5869, 5888};
const uint8_t FREQ5F_TABLE[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x40, 0x41, 0x42, 0x43};
const uint8_t FREQ60_TABLE[] = {0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9e, 0x9e, 0x9e, 0x9e};

/*!< Value for RF and analog modules switch (p10). */
const uint8_t RF_OFF_TABLE[] = {0x46, 0xaa, 0x50};
const uint8_t RF_ON_TABLE[] = {0x45, 0x55, 0xA0};
const uint8_t RF_REG_ADDR[] = {0x5d, 0x62, 0x51};

/*!< Registers of Lighting delay time. Unit: ms, min 2s (p8) */
const uint8_t HIGH_LEVEL_DELAY_CONTROL_ADDR = 0x41; /*!< Time_flag_out_ctrl 0x01 */
const uint8_t HIGH_LEVEL_DELAY_VALUE_ADDR = 0x42;   /*!< Time_flag_out_1 Bit<7:0> */

const uint8_t RESET_ADDR = 0x00;

/*!< Sensing distance address */
const uint8_t SIGNAL_DETECTION_THRESHOLD_ADDR_LO = 0x10;
const uint8_t SIGNAL_DETECTION_THRESHOLD_ADDR_HI = 0x11;

/*!< Bit field value for power registers */
const uint8_t POWER_THRESHOLD_ADDR_HI = 0x68;
const uint8_t POWER_THRESHOLD_ADDR_LO = 0x67;
const uint8_t PWR_WORK_TIME_EN = 8;     // Reg 0x67
const uint8_t PWR_BURST_TIME_EN = 32;   // Reg 0x68
const uint8_t PWR_THRESH_EN = 64;       // Reg 0x68
const uint8_t PWR_THRESH_VAL_EN = 128;  // Reg 0x67

/*!< Times */
const uint8_t TRIGGER_BASE_TIME_ADDR = 0x3D;  // 4 bytes, so up to 0x40
const uint8_t PROTECT_TIME_ADDR = 0x4E;       // 2 bytes, up to 0x4F
const uint8_t TRIGGER_KEEP_TIME_ADDR = 0x42;  // 4 bytes, so up to 0x45
const uint8_t TIME41_VALUE = 1;
const uint8_t SELF_CHECK_TIME_ADDR = 0x38;  // 2 bytes, up to 0x39

namespace esphome {
namespace at581x {

static const char *const TAG = "at581x";

bool AT581XComponent::i2c_write_reg(uint8_t addr, uint8_t data) {
  return this->write_register(addr, &data, 1) == esphome::i2c::NO_ERROR;
}
bool AT581XComponent::i2c_write_reg(uint8_t addr, uint32_t data) {
  return this->i2c_write_reg(addr + 0, uint8_t(data & 0xFF)) &&
         this->i2c_write_reg(addr + 1, uint8_t((data >> 8) & 0xFF)) &&
         this->i2c_write_reg(addr + 2, uint8_t((data >> 16) & 0xFF)) &&
         this->i2c_write_reg(addr + 3, uint8_t((data >> 24) & 0xFF));
}
bool AT581XComponent::i2c_write_reg(uint8_t addr, uint16_t data) {
  return this->i2c_write_reg(addr, uint8_t(data & 0xFF)) && this->i2c_write_reg(addr + 1, uint8_t((data >> 8) & 0xFF));
}

bool AT581XComponent::i2c_read_reg(uint8_t addr, uint8_t &data) {
  return this->read_register(addr, &data, 1) == esphome::i2c::NO_ERROR;
}

void AT581XComponent::setup() { ESP_LOGCONFIG(TAG, "Setting up AT581X..."); }
void AT581XComponent::dump_config() { LOG_I2C_DEVICE(this); }
#define ARRAY_SIZE(X) (sizeof(X) / sizeof((X)[0]))
bool AT581XComponent::i2c_write_config() {
  ESP_LOGCONFIG(TAG, "Writing new config for AT581X...");
  ESP_LOGCONFIG(TAG, "Frequency: %dMHz", this->freq_);
  ESP_LOGCONFIG(TAG, "Sensing distance: %d", this->delta_);
  ESP_LOGCONFIG(TAG, "Power: %dµA", this->power_);
  ESP_LOGCONFIG(TAG, "Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "Trigger base time: %dms", this->trigger_base_time_ms_);
  ESP_LOGCONFIG(TAG, "Trigger keep time: %dms", this->trigger_keep_time_ms_);
  ESP_LOGCONFIG(TAG, "Protect time: %dms", this->protect_time_ms_);
  ESP_LOGCONFIG(TAG, "Self check time: %dms", this->self_check_time_ms_);

  // Set frequency point
  if (!this->i2c_write_reg(FREQ_ADDR, GAIN61_VALUE)) {
    ESP_LOGE(TAG, "Failed to write AT581X Freq mode");
    return false;
  }
  // Find the current frequency from the table to know what value to write
  for (size_t i = 0; i < ARRAY_SIZE(FREQ_TABLE) + 1; i++) {
    if (i == ARRAY_SIZE(FREQ_TABLE)) {
      ESP_LOGE(TAG, "Set frequency not found");
      return false;
    }
    if (FREQ_TABLE[i] == this->freq_) {
      if (!this->i2c_write_reg(0x5F, FREQ5F_TABLE[i]) || !this->i2c_write_reg(0x60, FREQ60_TABLE[i])) {
        ESP_LOGE(TAG, "Failed to write AT581X Freq value");
        return false;
      }
      break;
    }
  }

  // Set distance
  if (!this->i2c_write_reg(SIGNAL_DETECTION_THRESHOLD_ADDR_LO, (uint8_t) (this->delta_ & 0xFF)) ||
      !this->i2c_write_reg(SIGNAL_DETECTION_THRESHOLD_ADDR_HI, (uint8_t) (this->delta_ >> 8))) {
    ESP_LOGE(TAG, "Failed to write AT581X sensing distance low");
    return false;
  }

  // Set power setting
  uint8_t pwr67 = PWR_THRESH_VAL_EN | PWR_WORK_TIME_EN, pwr68 = PWR_BURST_TIME_EN | PWR_THRESH_EN;
  for (size_t i = 0; i < ARRAY_SIZE(POWER_TABLE) + 1; i++) {
    if (i == ARRAY_SIZE(POWER_TABLE)) {
      ESP_LOGE(TAG, "Set power not found");
      return false;
    }
    if (POWER_TABLE[i] == this->power_) {
      pwr67 |= POWER67_TABLE[i];
      pwr68 |= POWER68_TABLE[i];  // See Page 12
      break;
    }
  }

  if (!this->i2c_write_reg(POWER_THRESHOLD_ADDR_LO, pwr67) || !this->i2c_write_reg(POWER_THRESHOLD_ADDR_HI, pwr68)) {
    ESP_LOGE(TAG, "Failed to write AT581X power registers");
    return false;
  }

  // Set gain
  if (!this->i2c_write_reg(GAIN_ADDR_TABLE[0], GAIN5C_TABLE[this->gain_]) ||
      !this->i2c_write_reg(GAIN_ADDR_TABLE[1], GAIN63_TABLE[this->gain_ >> 1])) {
    ESP_LOGE(TAG, "Failed to write AT581X gain registers");
    return false;
  }

  // Set times
  if (!this->i2c_write_reg(TRIGGER_BASE_TIME_ADDR, (uint32_t) this->trigger_base_time_ms_)) {
    ESP_LOGE(TAG, "Failed to write AT581X trigger base time registers");
    return false;
  }
  if (!this->i2c_write_reg(TRIGGER_KEEP_TIME_ADDR, (uint32_t) this->trigger_keep_time_ms_)) {
    ESP_LOGE(TAG, "Failed to write AT581X trigger keep time registers");
    return false;
  }

  if (!this->i2c_write_reg(PROTECT_TIME_ADDR, (uint16_t) this->protect_time_ms_)) {
    ESP_LOGE(TAG, "Failed to write AT581X protect time registers");
    return false;
  }
  if (!this->i2c_write_reg(SELF_CHECK_TIME_ADDR, (uint16_t) this->self_check_time_ms_)) {
    ESP_LOGE(TAG, "Failed to write AT581X self check time registers");
    return false;
  }

  if (!this->i2c_write_reg(0x41, TIME41_VALUE)) {
    ESP_LOGE(TAG, "Failed to enable AT581X time registers");
    return false;
  }

  // Don't know why it's required in other code, it's not in datasheet
  if (!this->i2c_write_reg(0x55, (uint8_t) 0x04)) {
    ESP_LOGE(TAG, "Failed to enable AT581X");
    return false;
  }

  // Ok, config is written, let's reset the chip so it's using the new config
  return this->reset_hardware_frontend();
}

// float AT581XComponent::get_setup_priority() const { return 0; }
bool AT581XComponent::reset_hardware_frontend() {
  if (!this->i2c_write_reg(RESET_ADDR, (uint8_t) 0) || !this->i2c_write_reg(RESET_ADDR, (uint8_t) 1)) {
    ESP_LOGE(TAG, "Failed to reset AT581X hardware frontend");
    return false;
  }
  return true;
}

void AT581XComponent::set_rf_mode(bool enable) {
  const uint8_t *p = enable ? &RF_ON_TABLE[0] : &RF_OFF_TABLE[0];
  for (size_t i = 0; i < ARRAY_SIZE(RF_REG_ADDR); i++) {
    if (!this->i2c_write_reg(RF_REG_ADDR[i], p[i])) {
      ESP_LOGE(TAG, "Failed to write AT581X RF mode");
      return;
    }
  }
}

}  // namespace at581x
}  // namespace esphome
