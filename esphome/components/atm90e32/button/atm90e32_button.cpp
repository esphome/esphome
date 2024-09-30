#include "atm90e32_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace atm90e32 {

static const char *const TAG = "atm90e32.button";

void ATM90E32CalibrationButton::press_action() {
  ESP_LOGI(TAG, "Running offset calibrations, Note: CTs and ACVs must be 0 during this process...");
  this->parent_->run_offset_calibrations();
}

void ATM90E32ClearCalibrationButton::press_action() {
  ESP_LOGI(TAG, "Offset calibrations cleared.");
  this->parent_->clear_offset_calibrations();
}

}  // namespace atm90e32
}  // namespace esphome
