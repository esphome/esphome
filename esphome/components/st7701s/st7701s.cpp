#ifdef USE_ESP32_VARIANT_ESP32S3
#include "st7701s.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7701s {

#ifdef USE_ESP_IDF
void ST7701S::dump_config() {
  ESP_LOGCONFIG("", "ST7701S RGB LCD");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  DE Pin: ", this->de_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++)
    ESP_LOGCONFIG(TAG, "  Data pin %d: %s", i, (this->data_pins_[i])->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

}  // namespace st7701s
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S3
