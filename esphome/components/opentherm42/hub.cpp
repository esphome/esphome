#include "hub.h"
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42";

void OpenTherm42Hub::setup() {
  this->datalink_ = make_unique<OpenThermDataLink>(this->in_pin_, this->out_pin_);
  if (!this->datalink_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize the OpenTherm datalink (%s); see previous log messages for details",
             timer_error_to_string(this->datalink_->get_timer_error()));
    this->mark_failed();
    return;
  }
}

void OpenTherm42Hub::loop() {
  // Conversation scheduling (§4.3: which data-id to talk about, and enforcing the timing between
  // conversations) is added once there's an application-layer data-id to actually request.
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
