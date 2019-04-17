#include "pcf8574_display.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lcd_pcf8574 {

static const char *TAG = "lcd_pcf8574";

void PCF8574LCDDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCF8574 LCD Display...");
  if (!this->write_bytes(0x08, nullptr, 0)) {
    this->mark_failed();
    return;
  }

  LCDDisplay::setup();
}
void PCF8574LCDDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "PCF8574 LCD Display:");
  ESP_LOGCONFIG(TAG, "  Columns: %u, Rows: %u", this->columns_, this->rows_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with LCD Display failed!");
  }
}
void PCF8574LCDDisplay::write_n_bits(uint8_t value, uint8_t n) {
  if (n == 4) {
    // Ugly fix: in the super setup() with n == 4 value needs to be shifted left
    value <<= 4;
  }
  uint8_t data = value | 0x08;  // Enable backlight
  this->write_bytes(data, nullptr, 0);
  // Pulse ENABLE
  this->write_bytes(data | 0x04, nullptr, 0);
  delayMicroseconds(1);  // >450ns
  this->write_bytes(data, nullptr, 0);
  delayMicroseconds(100);  // >37us
}
void PCF8574LCDDisplay::send(uint8_t value, bool rs) {
  this->write_n_bits((value & 0xF0) | rs, 0);
  this->write_n_bits(((value << 4) & 0xF0) | rs, 0);
}

}  // namespace lcd_pcf8574
}  // namespace esphome
