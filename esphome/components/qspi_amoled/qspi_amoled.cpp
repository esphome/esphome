//
// Created by Clyde Stubbs on 29/10/2023.
//
#include "qspi_amoled.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qspi_amoled {

#ifdef USE_ESP_IDF
void QspiAmoLed::dump_config() {
  ESP_LOGCONFIG("", "QSPI AMOLED");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
#ifdef USE_POWER_SUPPLY
  ESP_LOGCONFIG(TAG, "  Power Supply Configured: yes");
#endif
}

#endif
}  // namespace qspi_amoled
}  // namespace esphome
