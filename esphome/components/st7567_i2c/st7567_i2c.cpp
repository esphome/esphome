#include "st7567_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7567_i2c {

static const char *const TAG = "st7567_i2c";

void I2CST7567::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C ST7567 display...");
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
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_().c_str());
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", YESNO(this->mirror_y_));
  ESP_LOGCONFIG(TAG, "  Invert Colors: %s", YESNO(this->invert_colors_));
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with I2C ST7567 failed!");
  }
}

void I2CST7567::command(uint8_t value) { this->write_byte(0x00, value); }

void HOT I2CST7567::write_display_data() {
  static const size_t MAX_BLOCK_SIZE = 64;

  size_t block_size = MAX_BLOCK_SIZE;
  if (this->get_width_internal() < MAX_BLOCK_SIZE) {
    block_size = this->get_width_internal();
  }

  this->command_set_start_line_();

  for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
    this->command(esphome::st7567_base::ST7567_PAGE_ADDR + y);                       // Set Page
    this->command(esphome::st7567_base::ST7567_COL_ADDR_H);                          // Set MSB Column address
    this->command(esphome::st7567_base::ST7567_COL_ADDR_L + this->get_offset_x_());  // Set LSB Column address

    for (uint8_t x = 0; x < (uint8_t) this->get_width_internal(); x += block_size) {
      this->write_register(esphome::st7567_base::ST7567_SET_START_LINE, &buffer_[y * this->get_width_internal() + x],
                           this->get_width_internal() - x > block_size ? block_size : this->get_width_internal() - x,
                           true);
    }
  }
}

}  // namespace st7567_i2c
}  // namespace esphome
