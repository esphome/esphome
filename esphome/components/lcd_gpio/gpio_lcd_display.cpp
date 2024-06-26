#include "gpio_lcd_display.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lcd_gpio {

static const char *const TAG = "lcd_gpio";

void GPIOLCDDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GPIO LCD Display...");
  this->rs_pin_->setup();  // OUTPUT
  this->rs_pin_->digital_write(false);
  if (this->rw_pin_ != nullptr) {
    this->rw_pin_->setup();  // OUTPUT
    this->rw_pin_->digital_write(false);
  }
  this->enable_pin_->setup();  // OUTPUT
  this->enable_pin_->digital_write(false);

  for (uint8_t i = 0; i < (uint8_t) (this->is_four_bit_mode() ? 4u : 8u); i++) {
    this->data_pins_[i]->setup();  // OUTPUT
    this->data_pins_[i]->digital_write(false);
  }
  LCDDisplay::setup();
}
void GPIOLCDDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO LCD Display:");
  ESP_LOGCONFIG(TAG, "  Columns: %u, Rows: %u", this->columns_, this->rows_);
  LOG_PIN("  RS Pin: ", this->rs_pin_);
  LOG_PIN("  RW Pin: ", this->rw_pin_);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);

  LOG_PIN("  Data Pin 0: ", data_pins_[0]);
  LOG_PIN("  Data Pin 1: ", data_pins_[1]);
  LOG_PIN("  Data Pin 2: ", data_pins_[2]);
  LOG_PIN("  Data Pin 3: ", data_pins_[3]);
  if (!is_four_bit_mode()) {
    LOG_PIN("  Data Pin 4: ", data_pins_[4]);
    LOG_PIN("  Data Pin 5: ", data_pins_[5]);
    LOG_PIN("  Data Pin 6: ", data_pins_[6]);
    LOG_PIN("  Data Pin 7: ", data_pins_[7]);
  }
  LOG_UPDATE_INTERVAL(this);
}
void GPIOLCDDisplay::write_n_bits(uint8_t value, uint8_t n) {
  for (uint8_t i = 0; i < n; i++)
    this->data_pins_[i]->digital_write(value & (1 << i));

  this->enable_pin_->digital_write(true);
  delayMicroseconds(1);  // >450ns
  this->enable_pin_->digital_write(false);
  delayMicroseconds(40);  // >37us
}
void GPIOLCDDisplay::send(uint8_t value, bool rs) {
  this->rs_pin_->digital_write(rs);

  if (this->is_four_bit_mode()) {
    this->write_n_bits(value >> 4, 4);
    this->write_n_bits(value, 4);
  } else {
    this->write_n_bits(value, 8);
  }
}

}  // namespace lcd_gpio
}  // namespace esphome
