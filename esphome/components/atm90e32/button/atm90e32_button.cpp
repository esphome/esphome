#include "atm90e32_button.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace atm90e32 {

static const char *const TAG = "atm90e32.button";

void ATM90E32GainCalibrationButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "[CALIBRATION] No meters assigned to Gain Calibration button [%s]", this->get_name().c_str());
    return;
  }

  ESP_LOGI(TAG, "%s", this->get_name().c_str());
  ESP_LOGI(TAG,
           "[CALIBRATION] Use gain_ct: & gain_voltage: under each phase_x: in your config file to save these values");
  this->parent_->run_gain_calibrations();
}

void ATM90E32ClearGainCalibrationButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "[CALIBRATION] No meters assigned to Clear Gain button [%s]", this->get_name().c_str());
    return;
  }

  ESP_LOGI(TAG, "%s", this->get_name().c_str());
  this->parent_->clear_gain_calibrations();
}

void ATM90E32OffsetCalibrationButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "[CALIBRATION] No meters assigned to Offset Calibration button [%s]", this->get_name().c_str());
    return;
  }

  ESP_LOGI(TAG, "%s", this->get_name().c_str());
  ESP_LOGI(TAG, "[CALIBRATION] **NOTE: CTs and ACVs must be 0 during this process. USB power only**");
  ESP_LOGI(TAG, "[CALIBRATION] Use offset_voltage: & offset_current: under each phase_x: in your config file to save "
                "these values");
  this->parent_->run_offset_calibrations();
}

void ATM90E32ClearOffsetCalibrationButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "[CALIBRATION] No meters assigned to Clear Offset button [%s]", this->get_name().c_str());
    return;
  }

  ESP_LOGI(TAG, "%s", this->get_name().c_str());
  this->parent_->clear_offset_calibrations();
}

void ATM90E32PowerOffsetCalibrationButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "[CALIBRATION] No meters assigned to Power Calibration button [%s]", this->get_name().c_str());
    return;
  }

  ESP_LOGI(TAG, "%s", this->get_name().c_str());
  ESP_LOGI(TAG, "[CALIBRATION] **NOTE: CTs must be 0 during this process. Voltage reference should be present**");
  ESP_LOGI(TAG, "[CALIBRATION] Use offset_active_power: & offset_reactive_power: under each phase_x: in your config "
                "file to save these values");
  this->parent_->run_power_offset_calibrations();
}

void ATM90E32ClearPowerOffsetCalibrationButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "[CALIBRATION] No meters assigned to Clear Power button [%s]", this->get_name().c_str());
    return;
  }

  ESP_LOGI(TAG, "%s", this->get_name().c_str());
  this->parent_->clear_power_offset_calibrations();
}

}  // namespace atm90e32
}  // namespace esphome
