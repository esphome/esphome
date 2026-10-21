#include "epaper_spi_t133a01.h"

namespace esphome::epaper_spi {

bool EPaperT133A01::reset() {
  for (auto *enable_pin : this->enable_pins_) {
    enable_pin->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    if (this->state_ == EPaperState::RESET) {
      this->reset_pin_->digital_write(false);
      return false;
    }
    this->reset_pin_->digital_write(true);
  }
  return true;
}

}  // namespace esphome::epaper_spi
