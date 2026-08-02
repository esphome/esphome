#include "status_led.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::status_led {

static const char *const TAG = "status_led";

static constexpr uint32_t ERROR_PERIOD_MS = 250;
static constexpr uint32_t ERROR_ON_MS = 150;
static constexpr uint32_t WARNING_PERIOD_MS = 1500;
static constexpr uint32_t WARNING_ON_MS = 250;

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
  // Use millis() rather than App.get_loop_component_start_time() because this loop is also
  // dispatched from Application::feed_wdt() during long blocking operations, where the cached
  // per-component timestamp doesn't advance and would freeze the blink pattern.
  const uint32_t now = millis();
  if ((app_state & STATUS_LED_ERROR) != 0u) {
    this->pin_->digital_write(now % ERROR_PERIOD_MS < ERROR_ON_MS);
  } else if ((app_state & STATUS_LED_WARNING) != 0u) {
    this->pin_->digital_write(now % WARNING_PERIOD_MS < WARNING_ON_MS);
  } else {
    this->pin_->digital_write(false);
    this->disable_loop();
  }
}
float StatusLED::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esphome::status_led
