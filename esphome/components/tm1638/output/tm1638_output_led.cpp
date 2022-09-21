#include "tm1638_output_led.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tm1638 {

static const char *const TAG = "tm1638.led";

void TM1638OutputLed::write_state(bool state) { tm1638_->set_led(led_, state); }

void TM1638OutputLed::dump_config() {
  LOG_BINARY_OUTPUT(this);
  ESP_LOGCONFIG(TAG, "  LED: %d", led_);
}

}  // namespace tm1638
}  // namespace esphome
