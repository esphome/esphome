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
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Flip X: %s", YESNO(this->flip_x_));
  ESP_LOGCONFIG(TAG, "  Flip Y: %s", YESNO(this->flip_y_));
  ESP_LOGCONFIG(TAG, "  Inverted Color: %s", YESNO(this->invert_));
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with I2C ST7567 failed!");
  }
}

void I2CST7567::command(uint8_t value) { this->write_byte(0x00, value); }

void HOT I2CST7567::write_display_data() {
  for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
    this->command(esphome::st7567_base::ST7567_PAGE_ADDR + y);  // Set Page
    this->command(esphome::st7567_base::ST7567_COL_ADDR_H);     // Set MSB Column address
    this->command(esphome::st7567_base::ST7567_COL_ADDR_L);     // Set LSB Column address

    static const size_t BLOCK_SIZE = 64;
    for (uint8_t x = 0; x < (uint8_t) this->get_width_internal(); x += BLOCK_SIZE) {
      this->write_register(esphome::st7567_base::ST7567_SET_START_LINE, &buffer_[y * this->get_width_internal() + x],
                           this->get_width_internal() - x > BLOCK_SIZE ? BLOCK_SIZE : this->get_width_internal() - x,
                           true);
    }
  }
}

}  // namespace st7567_i2c
}  // namespace esphome
