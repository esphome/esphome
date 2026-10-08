#include "tmc2209.h"
#include "esphome/core/log.h"

namespace esphome::tmc2209 {

static const char *const TAG = "tmc2209";

void TMC2209Stepper::dump_config() {
  ESP_LOGCONFIG(TAG, "TMC2209:");
  TMC22XXStepper::dump_config();
}

}  // namespace esphome::tmc2209
