#include "st7567_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7567_i2c {

static const char *const TAG = "st7567_i2c";

void I2CST7567::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I²C ST7567 display...");

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  ST7567::setup();
}

void I2CST7567::dump_config() {
  ST7567::dump_config();
  ESP_LOGCONFIG(TAG, "  Interface: I²C");
  LOG_I2C_DEVICE(this);
  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with I2C ST7567 failed!");
  }
}

void I2CST7567::command_(uint8_t value) { this->write_byte(0x00, value); }

void HOT I2CST7567::write_display_data_() {
  static const size_t MAX_BLOCK_SIZE = 64;

  uint8_t max_y = this->device_config_.memory_height;
  uint8_t max_x = this->device_config_.memory_width;
  uint8_t vis_w = this->device_config_.visible_width;
  uint8_t off_x = this->get_visible_area_offset_x_();

  size_t block_size = MAX_BLOCK_SIZE;
  if (vis_w < MAX_BLOCK_SIZE) {
    block_size = vis_w;
  }
  // offset y - implemented in HW by selecting which line shown first
  // offset x - by drawing pixel with the offset
  this->command_set_start_line_();

  for (uint8_t y = 0; y < max_y / 8; y++) {
    this->command_(esphome::st7567_base::ST7567_PAGE_ADDR + y);       // Set Page
    this->command_(esphome::st7567_base::ST7567_COL_ADDR_H);          // Set MSB Column address
    this->command_(esphome::st7567_base::ST7567_COL_ADDR_L + off_x);  // Set LSB Column address

    for (uint8_t x = 0; x < vis_w; x += block_size) {
      this->write_register(esphome::st7567_base::ST7567_SET_START_LINE, &buffer_[y * max_x + x],
                           vis_w - x > block_size ? block_size : vis_w - x, true);
    }
  }
}

}  // namespace st7567_i2c
}  // namespace esphome
