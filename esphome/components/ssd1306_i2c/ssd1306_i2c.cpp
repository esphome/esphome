#include "ssd1306_i2c.h"
#include "esphome/core/log.h"

namespace esphome::ssd1306_i2c {

static const char *const TAG = "ssd1306_i2c";

void I2CSSD1306::setup() {
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
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  External VCC: %s\n"
                "  Flip X: %s\n"
                "  Flip Y: %s\n"
                "  Offset X: %d\n"
                "  Offset Y: %d\n"
                "  Inverted Color: %s",
                LOG_STR_ARG(this->model_str_()), YESNO(this->external_vcc_), YESNO(this->flip_x_), YESNO(this->flip_y_),
                this->offset_x_, this->offset_y_, YESNO(this->invert_));
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}
void I2CSSD1306::command(uint8_t value) { this->write_byte(0x00, value); }
void HOT I2CSSD1306::write_display_data() {
  if (this->is_sh1106_() || this->is_sh1107_()) {
    uint32_t i = 0;
    // Some panels wire their visible columns to a window of the controller RAM
    // that does not start at column 0 (e.g. SH1107 M5Stack Unit OLED needs offset_x: 32).
    // SH1106 keeps its historical 0x02 base column on top of any offset.
    uint8_t start_column = this->offset_x_;
    if (this->is_sh1106_()) {
      start_column += 0x02;
    }
    for (uint8_t page = 0; page < (uint8_t) this->get_height_internal() / 8; page++) {
      this->command(0xB0 + page);                 // row
      this->command(start_column & 0x0F);         // lower column
      this->command(0x10 | (start_column >> 4));  // higher column
      for (uint8_t x = 0; x < (uint8_t) this->get_width_internal() / 16; x++) {
        uint8_t data[16];
        for (uint8_t &j : data)
          j = this->buffer_[i++];
        this->write_bytes(0x40, data, sizeof(data));
      }
    }
  } else {
    size_t block_size = 16;
    if ((this->get_buffer_length_() % 24) == 0) {
      // use 24 byte block size for e.g. 72x40 displays where buffer size is multiple of 24, not 16
      block_size = 24;
    }

    for (uint32_t i = 0; i < this->get_buffer_length_();) {
      uint8_t data[block_size];
      for (uint8_t &j : data)
        j = this->buffer_[i++];
      this->write_bytes(0x40, data, sizeof(data));
    }
  }
}

}  // namespace esphome::ssd1306_i2c
