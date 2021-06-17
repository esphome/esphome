#include "ttp229_bsf.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ttp229_bsf {

static const char *const TAG = "ttp229_bsf";

void TTP229BSFComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ttp229_bsf... ");
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

}  // namespace ttp229_bsf
}  // namespace esphome
