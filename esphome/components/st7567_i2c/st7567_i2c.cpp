#include "st7567_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7567_i2c {

static const char *const TAG = "st7567_i2c";

void I2CST7567::setup() {
  this->init_reset_();

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  ST7567::setup();
}

void I2CST7567::dump_config() {
  LOG_DISPLAY("", "I2CST7567", this);
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG,
                "  Mirror X: %s\n"
                "  Mirror Y: %s\n"
                "  Invert Colors: %s",
                YESNO(this->mirror_x_), YESNO(this->mirror_y_), YESNO(this->invert_colors_));
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void I2CST7567::command(uint8_t value) { this->write_byte(0x00, value); }

void HOT I2CST7567::write_display_data() {
  // ST7567A has built-in RAM with 132x65 bit capacity which stores the display data.
  // but only first 128 pixels from each line are shown on screen
  // if screen got flipped horizontally then it shows last 128 pixels,
  // so we need to write x coordinate starting from column 4, not column 0
  this->command(esphome::st7567_base::ST7567_SET_START_LINE + this->start_line_);
  for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
    this->command(esphome::st7567_base::ST7567_PAGE_ADDR + y);                       // Set Page
    this->command(esphome::st7567_base::ST7567_COL_ADDR_H);                          // Set MSB Column address
    this->command(esphome::st7567_base::ST7567_COL_ADDR_L + this->get_offset_x_());  // Set LSB Column address

    static const size_t BLOCK_SIZE = 64;
    for (uint8_t x = 0; x < (uint8_t) this->get_width_internal(); x += BLOCK_SIZE) {
      size_t remaining = static_cast<size_t>(this->get_width_internal()) - x;
      size_t chunk = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
      this->write_register(esphome::st7567_base::ST7567_SET_START_LINE, &buffer_[y * this->get_width_internal() + x],
                           chunk);
    }
  }
}

}  // namespace st7567_i2c
}  // namespace esphome
