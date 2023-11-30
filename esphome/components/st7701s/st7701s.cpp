//
// Created by Clyde Stubbs on 29/10/2023.
//
#include "st7701s.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7701s {


void ST7701S::dump_config() {
  ESP_LOGCONFIG("", "ST7701S RGB LCD");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
#ifdef USE_POWER_SUPPLY
  ESP_LOGCONFIG(TAG, "  Power Supply Configured: yes");
#endif
}

}  // namespace st7701s
}  // namespace esphome
