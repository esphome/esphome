#include "ssd1325_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ssd1325_i2c {

static const char *TAG = "ssd1325_i2c";

void I2CSSD1325::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C SSD1325...");
  this->init_reset_();

  this->parent_->raw_begin_transmission(this->address_);
  if (!this->parent_->raw_end_transmission(this->address_)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  SSD1325::setup();
}
void I2CSSD1325::dump_config() {
  LOG_DISPLAY("", "I2C SSD1325", this);
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  External VCC: %s", YESNO(this->external_vcc_));
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with SSD1325 failed!");
  }
}
void I2CSSD1325::command(uint8_t value) { this->write_byte(0x00, value); }
void HOT I2CSSD1325::write_display_data() {
  for (uint32_t i = 0; i < this->get_buffer_length_();) {
    uint8_t data[16];
    for (uint8_t &j : data)
      j = this->buffer_[i++];
    this->write_bytes(0x40, data, sizeof(data));
  }
}

}  // namespace ssd1325_i2c
}  // namespace esphome
