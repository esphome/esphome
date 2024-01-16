#ifdef USE_ESP_IDF
#include "rpi_dpi_rgb.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rpi_dpi_rgb {

void RpiDpiRgb::dump_config() {
  ESP_LOGCONFIG("", "RPI_DPI_RGB LCD");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  DE Pin: ", this->de_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++)
    ESP_LOGCONFIG(TAG, "  Data pin %d: %s", i, (this->data_pins_[i])->dump_summary().c_str());
}

}  // namespace rpi_dpi_rgb
}  // namespace esphome

#endif  // USE_ESP_IDF
