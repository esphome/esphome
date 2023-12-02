#include "status_led.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace status_led {

static const char *const TAG = "status_led";

StatusLED *global_status_led = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

StatusLED::StatusLED(GPIOPin *pin) : pin_(pin) { global_status_led = this; }
void StatusLED::pre_setup() {
  ESP_LOGCONFIG(TAG, "Setting up Status LED...");
  this->pin_->setup();
  this->pin_->digital_write(false);
}
void StatusLED::dump_config() {
  ESP_LOGCONFIG(TAG, "Status LED:");
  LOG_PIN("  Pin: ", this->pin_);
}
void StatusLED::loop() {
  if ((App.get_app_state() & STATUS_LED_ERROR) != 0u) {
    this->pin_->digital_write(millis() % 250u < 150u);
  } else if ((App.get_app_state() & STATUS_LED_WARNING) != 0u) {
    this->pin_->digital_write(millis() % 1500u < 250u);
  } else {
    this->pin_->digital_write(false);
  }
}
float StatusLED::get_setup_priority() const { return setup_priority::HARDWARE; }
float StatusLED::get_loop_priority() const { return 50.0f; }

}  // namespace status_led
}  // namespace esphome
