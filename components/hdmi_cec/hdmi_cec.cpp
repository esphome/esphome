#include "hdmi_cec.h"

#include "esphome/core/log.h"

namespace esphome {
namespace hdmi_cec {

static const char *const TAG = "hdmi_cec";

void HDMICEC::setup() {
  this->cec_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->cec_pin_->attach_interrupt(HDMICEC::interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  LOG_PIN("  Pin: ", this->cec_pin_);
}

void IRAM_ATTR HDMICEC::interrupt(HDMICEC* self) {
  // TODO
}

}
}
