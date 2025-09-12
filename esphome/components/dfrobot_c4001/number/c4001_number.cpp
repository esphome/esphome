#include "c4001_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4001 {

static const char *TAG = "c4001.number";

// -------- Min Range --------
void MinRangeNumber::control(float value) {
  if (this->parent_) {
    float max_range = this->parent_->get_max_range();
    float min_range = this->parent_->get_min_range();
    if(value < max_range){
      ESP_LOGD(TAG, "Set min range to %.1f", value);
      this->publish_state(value);
      this->parent_->set_min_range(value);
    }else{
      this->publish_state(NAN);
      this->publish_state(min_range);
    }
  }
  
}

// -------- Max Range --------
void MaxRangeNumber::control(float value) {
  if (this->parent_) {
    float min_range = this->parent_->get_min_range();    
    float max_range = this->parent_->get_max_range(); 
    if(value > min_range){
      this->publish_state(value);
      ESP_LOGD(TAG, "Set max range to %.1f", value);
      this->parent_->set_max_range(value);
    }else{
      this->publish_state(NAN);
      this->publish_state(max_range);
    }
  }
}

// -------- Trigger Range --------
void TrigRangeNumber::control(float value) {
  if (this->parent_) {
    this->parent_->set_trig_range(value);
    ESP_LOGD(TAG, "set_trig_range to %.1f", value);
  }
  this->publish_state(value);
}

// -------- Keep Sensitivity --------
void KeepSensitivityNumber::control(float value) {
  if (this->parent_) {
    this->parent_->set_keep_sensitivity(value);
    ESP_LOGD(TAG, "Set keep sensitivity to %.1f", value);
  }
  this->publish_state(value);
}

// -------- Trigger Sensitivity --------
void TrigSensitivityNumber::control(float value) {
  if (this->parent_) {
    this->parent_->set_trig_sensitivity(value);
    ESP_LOGD(TAG, "Set trigger sensitivity to %.1f", value);
  }
  this->publish_state(value);
}

// -------- Confirm Delay --------
void ConfirmDelayNumber::control(float value) {
  if (this->parent_) {
    this->parent_->set_confirm_delay(value);
    ESP_LOGD(TAG, "Set confirm delay to %.1f s", value);
  }
  this->publish_state(value);
}

// -------- Disappear Delay --------
void DisappearDelayNumber::control(float value) {
  if (this->parent_) {
    this->parent_->set_disappear_delay(value);
    ESP_LOGD(TAG, "Set disappear delay to %.1f s", value);
  }
  this->publish_state(value);
}

// -------- Threshold Factor --------
void ThresholdFactorNumber::control(float value) {
  if (this->parent_) {
    this->parent_->set_threshold_factor(value);
    ESP_LOGD(TAG, "Set threshold factor to %.1f", value);
  }
  this->publish_state(value);
}

}  // namespace dfrobot_c4001
}  // namespace esphome
