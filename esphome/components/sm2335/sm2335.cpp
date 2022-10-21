#include "sm2335.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sm2335 {

static const char *const TAG = "sm2335";

// 11 = identification | 0 = reserved | 00 = Standby | 000 = start at OUT1/5
static const uint8_t SM2335_ADDR_STANDBY = 0xC0;  // 11000000 0xC0
// 11 = identification | 0 = reserved | 01 = 3 channels (RGB) | 000 = start at OUT1/5
static const uint8_t SM2335_ADDR_START_3CH = 0xC8;  // 11001000 0xC8
// 11 = identification | 0 = reserved | 10 = 2 channels (CW) | 000 = start at OUT1/5
static const uint8_t SM2335_ADDR_START_2CH = 0xD0;  // 11010000 0xD0
// 11 = identification | 0 = reserved | 11 = 5 channels (RGB+CW) | 000 = start at OUT1/5
static const uint8_t SM2335_ADDR_START_5CH = 0xD8;  // 11011000 0xD8

// Power current values
// HEX | Binary | RGB level | White level | Config value
// 0x0 | 0000   | RGB 10mA  | CW 5mA      | 0
// 0x1 | 0001   | RGB 20mA  | CW 10mA     | 1
// 0x2 | 0010   | RGB 30mA  | CW 15mA     | 2 - Default spec color value
// 0x3 | 0011   | RGB 40mA  | CW 20mA     | 3
// 0x4 | 0100   | RGB 50mA  | CW 25mA     | 4 - Default spec white value
// 0x5 | 0101   | RGB 60mA  | CW 30mA     | 5
// 0x6 | 0110   | RGB 70mA  | CW 35mA     | 6
// 0x7 | 0111   | RGB 80mA  | CW 40mA     | 7
// 0x8 | 1000   | RGB 90mA  | CW 45mA     | 8
// 0x9 | 1001   | RGB 100mA | CW 50mA     | 9
// 0xA | 1010   | RGB 110mA | CW 55mA     | 10
// 0xB | 1011   | RGB 120mA | CW 60mA     | 11
// 0xC | 1100   | RGB 130mA | CW 65mA     | 12
// 0xD | 1101   | RGB 140mA | CW 70mA     | 13
// 0xE | 1110   | RGB 150mA | CW 75mA     | 14
// 0xF | 1111   | RGB 160mA | CW 80mA     | 15

void SM2335::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SM2335 Output Component...");
  this->data_pin_->setup();
  this->data_pin_->digital_write(true);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(true);
  this->pwm_amounts_.resize(5, 0);
}
void SM2335::dump_config() {
  ESP_LOGCONFIG(TAG, "SM2335:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
  ESP_LOGCONFIG(TAG, "  Color Channels Max Power: %u", this->max_power_color_channels_);
  ESP_LOGCONFIG(TAG, "  White Channels Max Power: %u", this->max_power_white_channels_);
}

void SM2335::loop() {
  if (!this->update_)
    return;

  uint8_t data[12];
  if (this->pwm_amounts_[0] == 0 && this->pwm_amounts_[1] == 0 && this->pwm_amounts_[2] == 0 &&
      this->pwm_amounts_[3] == 0 && this->pwm_amounts_[4] == 0) {
    // Off / Sleep
    data[0] = SM2335_ADDR_STANDBY;
    for (int i = 1; i < 12; i++)
      data[i] = 0;
    this->write_buffer_(data, 12);
  } else if (this->pwm_amounts_[0] == 0 && this->pwm_amounts_[1] == 0 && this->pwm_amounts_[2] == 0 &&
             (this->pwm_amounts_[3] > 0 || this->pwm_amounts_[4] > 0)) {
    // Only data on white channels
    data[0] = SM2335_ADDR_START_2CH;
    data[1] = 0 << 4 | this->max_power_white_channels_;
    for (int i = 2, j = 0; i < 12; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] >> 0x8;
      data[i + 1] = this->pwm_amounts_[j] & 0xFF;
    }
    this->write_buffer_(data, 12);
  } else if ((this->pwm_amounts_[0] > 0 || this->pwm_amounts_[1] > 0 || this->pwm_amounts_[2] > 0) &&
             this->pwm_amounts_[3] == 0 && this->pwm_amounts_[4] == 0) {
    // Only data on RGB channels
    data[0] = SM2335_ADDR_START_3CH;
    data[1] = this->max_power_color_channels_ << 4 | 0;
    for (int i = 2, j = 0; i < 12; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] >> 0x8;
      data[i + 1] = this->pwm_amounts_[j] & 0xFF;
    }
    this->write_buffer_(data, 12);
  } else {
    // All channels
    data[0] = SM2335_ADDR_START_5CH;
    data[1] = this->max_power_color_channels_ << 4 | this->max_power_white_channels_;
    for (int i = 2, j = 0; i < 12; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] >> 0x8;
      data[i + 1] = this->pwm_amounts_[j] & 0xFF;
    }
    this->write_buffer_(data, 12);
  }

  this->update_ = false;
}

}  // namespace sm2335
}  // namespace esphome
