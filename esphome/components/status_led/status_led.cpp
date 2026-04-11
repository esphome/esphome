#include "status_led.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace status_led {

static const char *const TAG = "status_led";

static constexpr uint32_t ERROR_ON_MS = 150;
static constexpr uint32_t ERROR_OFF_MS = 100;
static constexpr uint32_t WARNING_ON_MS = 250;
static constexpr uint32_t WARNING_OFF_MS = 1250;

StatusLED *global_status_led = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

StatusLED::StatusLED(GPIOPin *pin) : pin_(pin) { global_status_led = this; }
void StatusLED::pre_setup() {
  this->pin_->setup();
  this->pin_->digital_write(false);
}
void StatusLED::dump_config() {
  ESP_LOGCONFIG(TAG, "Status LED:");
  LOG_PIN("  Pin: ", this->pin_);
}
void StatusLED::loop() {
  const uint32_t app_state = App.get_app_state();
  const bool error = (app_state & STATUS_LED_ERROR) != 0u;
  const bool warning = (app_state & STATUS_LED_WARNING) != 0u;
  if (!error && !warning) {
    if (this->led_on_) {
      this->led_on_ = false;
      this->pin_->digital_write(false);
    }
    this->disable_loop();
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if ((int32_t) (now - this->next_toggle_) < 0)
    return;
  this->led_on_ = !this->led_on_;
  uint32_t delay;
  if (error) {
    delay = this->led_on_ ? ERROR_ON_MS : ERROR_OFF_MS;
  } else {
    delay = this->led_on_ ? WARNING_ON_MS : WARNING_OFF_MS;
  }
  this->next_toggle_ = now + delay;
  this->pin_->digital_write(this->led_on_);
}
float StatusLED::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace status_led
}  // namespace esphome
