#include "m5ioe1_switch.h"
#include "esphome/core/log.h"

namespace esphome::m5ioe1 {

static const char *const TAG = "m5ioe1.switch";

void AW8737ASwitch::setup() { this->parent_->setup_aw8737a(this->pin_, this->pulse_num_); }

void AW8737ASwitch::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5IOE1 Switch:\n"
                "  Pin: IO%d\n"
                "  Pulse number: %d",
                this->pin_, this->pulse_num_);
  LOG_SWITCH("  ", "AW8737A Switch", this);
}

void AW8737ASwitch::write_state(bool state) { this->parent_->start_aw8737a(state); }

}  // namespace esphome::m5ioe1
