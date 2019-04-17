#include "ssd1306_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ssd1306_i2c {

static const char *TAG = "ssd1306_i2c";

void I2CSSD1306::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C SSD1306...");
  this->init_reset_();

  this->parent_->raw_begin_transmission(this->address_);
  if (!this->parent_->raw_end_transmission(this->address_)) {
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
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with SSD1306 failed!");
  }
}
void I2CSSD1306::command(uint8_t value) { this->write_byte(0x00, value); }
void HOT I2CSSD1306::write_display_data() {
  if (this->is_sh1106_()) {
    uint32_t i = 0;
    for (uint8_t page = 0; page < this->get_height_internal() / 8; page++) {
      this->command(0xB0 + page);  // row
      this->command(0x02);         // lower column
      this->command(0x10);         // higher column

      for (uint8_t x = 0; x < this->get_width_internal() / 16; x++) {
        uint8_t data[16];
        for (uint8_t &j : data)
          j = this->buffer_[i++];
        this->write_bytes(0x40, data, sizeof(data));
      }
    }
  } else {
    for (uint32_t i = 0; i < this->get_buffer_length_();) {
      uint8_t data[16];
      for (uint8_t &j : data)
        j = this->buffer_[i++];
      this->write_bytes(0x40, data, sizeof(data));
    }
  }
}

}  // namespace ssd1306_i2c
}  // namespace esphome
