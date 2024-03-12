#include "bh1745_switch_led.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bh1745 {

static const char *const TAG = "bh1745.led";

void BH1745SwitchLed::write_state(bool state) {
  bh1745_->switch_led(state);
  publish_state(state);
}

void BH1745SwitchLed::dump_config() { LOG_SWITCH("", "BH1745 Pimoroni LED", this); }

}  // namespace bh1745
}  // namespace esphome
