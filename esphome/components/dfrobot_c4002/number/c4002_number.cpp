#include "c4002_number.h"
#include "esphome/core/log.h"
namespace esphome {
namespace dfrobot_c4002 {
static const char *const TAG = "c4002.number";



// -------- Min Range --------
void MinDetectRangeNumber::control(float value) {
  if (this->parent_) {
    float max_range = this->parent_->get_max_detect_range_number();
    float min_range = this->parent_->get_min_detect_range_number();
    if (value < max_range) {
      ESP_LOGD(TAG, "Set min range to %.1f", value);
      if(this->parent_->set_min_range(value)){
        ESP_LOGD(TAG, "Set min range success");
        this->publish_state(value);
      }else{
        ESP_LOGD(TAG, "Set min range failed");
        this->publish_state(NAN);
      }
    } else {
      this->publish_state(min_range);
    }
  }
}
// -------- Max Range --------
void MaxDetectRangeNumber::control(float value) {
  if (this->parent_) {
    float min_range = this->parent_->get_min_detect_range_number();
    float max_range = this->parent_->get_max_detect_range_number();
    if (value > min_range) {
      ESP_LOGD(TAG, "Set max range to %.1f", value);
      if(this->parent_->set_max_range(value)){
        ESP_LOGD(TAG, "Set max range success");
        this->publish_state(value);
      }else{
        ESP_LOGD(TAG, "Set max range failed");
        this->publish_state(NAN);
      }
    } else {
      this->publish_state(max_range);
    }
  }
}
void LightThresholdNumber::control(float value) {
  if (this->parent_) {
    ESP_LOGD(TAG, "Set light threshold to %.1f", value);

    if(this->parent_->setLightThreshold(value)){
      ESP_LOGD(TAG, "Set light threshold success");
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set light threshold failed");
      this->publish_state(NAN);
    }
  }
}

// ===== 区域 1 =====
void Area1MinRangeNumber::control(float value) {

  float area1_min = this->parent_->get_area_range(AREA1_DOOR_MIN);
  float area1_max = this->parent_->get_area_range(AREA1_DOOR_MAX);

  if(value < area1_max){
    if(this->parent_->joint_enable_door()){
      ESP_LOGD(TAG, "Set area 1 min range to %.1f", value);
      this->parent_->set_area_range(AREA1_DOOR_MIN, value);
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set area 1 min range failed");
      this->publish_state(NAN);
    }
  }else{
    this->publish_state(area1_min);
  }
}

void Area1MaxRangeNumber::control(float value) {
  float area1_min = this->parent_->get_area_range(AREA1_DOOR_MIN);
  float area1_max = this->parent_->get_area_range(AREA1_DOOR_MAX);
  if(value > area1_min){
    if(this->parent_->joint_enable_door()){
      ESP_LOGD(TAG, "Set area 1 max range to %.1f", value);
      this->parent_->set_area_range(AREA1_DOOR_MAX, value);
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set area 1 max range failed");
      this->publish_state(NAN);
    }
  }else{
    this->publish_state(area1_max);
  }

  this->publish_state(value);
}

// ===== 区域 2 =====
void Area2MinRangeNumber::control(float value) {
  float area2_min = this->parent_->get_area_range(AREA2_DOOR_MIN);
  float area2_max = this->parent_->get_area_range(AREA2_DOOR_MAX);

  if(value < area2_max){
    if(this->parent_->joint_enable_door()){
      ESP_LOGD(TAG, "Set area 2 min range to %.1f", value);
      this->parent_->set_area_range(AREA2_DOOR_MIN, value);
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set area 2 min range failed");
      this->publish_state(NAN);
    }
  }else{
    this->publish_state(area2_min);
    }

  
}

void Area2MaxRangeNumber::control(float value) {
  float area2_min = this->parent_->get_area_range(AREA2_DOOR_MIN);
  float area2_max = this->parent_->get_area_range(AREA2_DOOR_MAX);

  if(value > area2_min){
    if(this->parent_->joint_enable_door()){
      ESP_LOGD(TAG, "Set area 2 max range to %.1f", value);
      this->parent_->set_area_range(AREA2_DOOR_MAX, value);
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set area 2 max range failed");
      this->publish_state(NAN);
    }
  }else{
    this->publish_state(area2_max);
  
  }
}

// ===== 区域 3 =====
void Area3MinRangeNumber::control(float value) {
  float area3_min = this->parent_->get_area_range(AREA3_DOOR_MIN);
  float area3_max = this->parent_->get_area_range(AREA3_DOOR_MAX);

  if(value < area3_max){
    if(this->parent_->joint_enable_door()){
      ESP_LOGD(TAG, "Set area 3 min range to %.1f", value);
      this->parent_->set_area_range(AREA3_DOOR_MIN, value);
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set area 3 min range failed");
      this->publish_state(NAN);
    }
  }else{
    this->publish_state(area3_min);
  }
}

void Area3MaxRangeNumber::control(float value) {
  float area3_min = this->parent_->get_area_range(AREA3_DOOR_MIN);
  float area3_max = this->parent_->get_area_range(AREA3_DOOR_MAX);

  if(value > area3_min){
    if(this->parent_->joint_enable_door()){
      ESP_LOGD(TAG, "Set area 3 max range to %.1f", value);
      this->parent_->set_area_range(AREA3_DOOR_MAX, value);
      this->publish_state(value);
    }else{
      ESP_LOGD(TAG, "Set area 3 max range failed");
      this->publish_state(NAN);
    }
  }else{
    this->publish_state(area3_max);
  }
}


}  // namespace dfrobot_c4002
}  // namespace esphome
