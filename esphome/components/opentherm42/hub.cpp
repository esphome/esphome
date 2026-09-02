#include "hub.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42";

void OpenTherm42Hub::setup() {
  this->in_pin_->setup();
  this->out_pin_->setup();
}

void OpenTherm42Hub::loop() {}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
