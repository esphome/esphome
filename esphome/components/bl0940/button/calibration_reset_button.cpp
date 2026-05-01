#include "calibration_reset_button.h"
#include "../bl0940.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace bl0940 {

static const char *const TAG = "bl0940.button.calibration_reset";

void CalibrationResetButton::dump_config() { LOG_BUTTON("", "Calibration Reset Button", this); }

void CalibrationResetButton::press_action() {
  ESP_LOGI(TAG, "Resetting calibration defaults...");
  this->parent_->reset_calibration();
}

}  // namespace bl0940
}  // namespace esphome
