#include "hdmi_cec.h"

#include "esphome/core/log.h"

namespace esphome {
namespace hdmi_cec {

static const char *const TAG = "hdmi_cec";

void HDMICEC::setup() {
  this->cec_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  LOG_PIN("  Pin: ", this->cec_pin_);
}

void HDMICEC::loop() {
  // TODO
}

}
}
