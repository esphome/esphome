#include "hdmi_cec.h"

#include "esphome/core/log.h"

namespace esphome {
namespace hdmi_cec {

static const char *const TAG = "hdmi_cec";

void HDMICEC::setup() {
  this->cec_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->cec_pin_->attach_interrupt(HDMICEC::falling_edge_interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
  this->cec_pin_->attach_interrupt(HDMICEC::rising_edge_interrupt, this, gpio::INTERRUPT_RISING_EDGE);
}

void HDMICEC::dump_config() {
  ESP_LOGCONFIG(TAG, "HDMI-CEC");
  LOG_PIN("  Pin: ", this->cec_pin_);
}

void IRAM_ATTR HDMICEC::falling_edge_interrupt(HDMICEC *self) {
  self->last_falling_edge_ms_ = millis();
  // TODO
}

void IRAM_ATTR HDMICEC::rising_edge_interrupt(HDMICEC *self) {
  if (self->last_falling_edge_ms_ == 0) {
    return;
  }

  auto pulse_duration = millis() - self->last_falling_edge_ms_;
  ESP_LOGI(TAG, "pulse duration: %zu", pulse_duration);
  // TODO

  self->last_falling_edge_ms_ = 0;
}

}
}
