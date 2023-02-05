#include "iec62056switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace iec62056 {

static const char *const TAG = "iec62056.switch";

void IEC62056Switch::write_state(bool state) {
  ESP_LOGV(TAG, "Setting switch to %s", ONOFF(state));
  if (state) {
    this->parent_->trigger_readout();
  }
  this->publish_state(state);
}

}  // namespace iec62056
}  // namespace esphome
