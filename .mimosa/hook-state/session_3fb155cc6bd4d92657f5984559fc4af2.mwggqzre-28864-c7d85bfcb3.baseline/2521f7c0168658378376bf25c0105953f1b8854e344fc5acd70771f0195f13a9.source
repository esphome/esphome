#include "ttp229_bsf.h"
#include "esphome/core/log.h"

namespace esphome::ttp229_bsf {

ESPHOME_LOG_TAG(TAG, "ttp229_bsf");

void TTP229BSFComponent::setup() {
  this->sdo_pin_->setup();
  this->scl_pin_->setup();
  this->scl_pin_->digital_write(true);
  delay(2);
}
void TTP229BSFComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ttp229:");
  LOG_PIN("  SCL pin: ", this->scl_pin_);
  LOG_PIN("  SDO pin: ", this->sdo_pin_);
}

}  // namespace esphome::ttp229_bsf
