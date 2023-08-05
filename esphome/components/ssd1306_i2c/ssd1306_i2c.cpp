#include "ssd1306_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ssd1306_i2c {

static const char *const TAG = "ssd1306_i2c";

void I2CSSD1306::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C SSD1306...");
  this->init_reset_();

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  SSD1306::setup();
}
void I2CSSD1306::dump_config() {
  LOG_DISPLAY("", "I2C SSD1306", this);
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  External VCC: %s", YESNO(this->external_vcc_));
  ESP_LOGCONFIG(TAG, "  Flip X: %s", YESNO(this->flip_x_));
  ESP_LOGCONFIG(TAG, "  Flip Y: %s", YESNO(this->flip_y_));
  ESP_LOGCONFIG(TAG, "  Offset X: %d", this->offset_x_);
  ESP_LOGCONFIG(TAG, "  Offset Y: %d", this->offset_y_);
  ESP_LOGCONFIG(TAG, "  Inverted Color: %s", YESNO(this->invert_));
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with SSD1306 failed!");
  }
}
void I2CSSD1306::command(uint8_t value) { this->write_byte(0x00, value); }
void HOT I2CSSD1306::write_display_data() {
  if (this->is_sh1106_() || this->is_sh1107_()) {
    uint32_t i = 0;
    for (uint8_t page = 0; page < (uint8_t) this->get_height_internal() / 8; page++) {
      this->command(0xB0 + page);  // row
      if (this->is_sh1106_()) {
        this->command(0x02);  // lower column - 0x02 is historical SH1106 value
      } else {
        // Other SH1107 drivers use 0x00
        // Column values dont change and it seems they can be set only once,
        // but we follow SH1106 implementation and resend them
        this->command(0x00);
      }
      this->command(0x10);  // higher column
      for (uint8_t x = 0; x < (uint8_t) this->get_width_internal() / 16; x++) {
        uint8_t data[16];
        for (uint8_t &j : data)
          j = this->buffer_[i++];
        this->write_bytes(0x40, data, sizeof(data));
      }
    }
  } else {
    size_t block_size = 16;
    if ((this->get_buffer_length_() & 8) == 8) {
      // use smaller block size for e.g. 72x40 displays where buffer size is multiple of 8, not 16
      block_size = 8;
    }

    for (uint32_t i = 0; i < this->get_buffer_length_();) {
      uint8_t data[block_size];
      for (uint8_t &j : data)
        j = this->buffer_[i++];
      this->write_bytes(0x40, data, sizeof(data));
    }
  }
}

}  // namespace ssd1306_i2c
}  // namespace esphome
