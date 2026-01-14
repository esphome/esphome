#include "c4002_number.h"
#include "esphome/core/log.h"
namespace esphome {
namespace dfrobot_c4002 {
static const char *const TAG = "dfrobot_c4002.number: ";

// -------- Min Range --------
void MinDetectRangeNumber::control(float value) {
  if (this->parent_) {
    float max_range = this->parent_->get_max_detect_range_number();
    float min_range = this->parent_->get_min_detect_range_number();
    if (value < max_range) {
      ESP_LOGD(TAG, "Set min range to %.1f", value);
      if (this->parent_->set_min_range(value)) {
        ESP_LOGD(TAG, "Set min range success");
        this->publish_state(value);
      } else {
        ESP_LOGD(TAG, "Set min range failed");
        this->publish_state(min_range);
      }
    } else {
      this->publish_state(min_range);
      // this->parent_->publish_text_("The maximum must be greater than the minimum.")
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
      if (this->parent_->set_max_range(value)) {
        ESP_LOGD(TAG, "Set max range success");
        this->publish_state(value);
      } else {
        ESP_LOGD(TAG, "Set max range failed");
        this->publish_state(max_range);
      }
    } else {
      this->publish_state(max_range);
      // this->parent_->publish_text_("The maximum must be greater than the minimum.")
    }
  }
}
// -------- Light Threshold --------
void LightThresholdNumber::control(float value) {
  if (this->parent_) {
    ESP_LOGD(TAG, "Set light threshold to %.1f", value);

    if (this->parent_->set_light_threshold(value)) {
      ESP_LOGD(TAG, "Set light threshold success");
      this->publish_state(value);
    } else {
      ESP_LOGD(TAG, "Set light threshold failed");
      this->publish_state(0.0);
    }
  }
}

// ===== 区域 1 =====
void Area1MinRangeNumber::control(float value) {
  float area1_min = this->parent_->get_area_range(AREA1_DOOR_MIN);
  float area1_max = this->parent_->get_area_range(AREA1_DOOR_MAX);

  if (value <= area1_max) {
    this->parent_->set_area_range(AREA1_DOOR_MIN, value);
    ESP_LOGD(TAG, "Set area 1 min range to %.1f", this->parent_->get_area_range(AREA1_DOOR_MIN));

    if (this->parent_->joint_enable_door()) {
      this->parent_->set_area_range(AREA1_DOOR_MIN, value);
      this->publish_state(value);

    } else {
      this->publish_state(area1_min);
      this->parent_->set_area_range(AREA1_DOOR_MIN, area1_min);
    }
  } else {
    this->publish_state(area1_min);
    this->parent_->set_area_range(AREA1_DOOR_MIN, area1_min);
    // this->parent_->publish_text_("The maximum must be greater than the minimum.")
  }
}

void Area1MaxRangeNumber::control(float value) {
  float area1_min = this->parent_->get_area_range(AREA1_DOOR_MIN);
  float area1_max = this->parent_->get_area_range(AREA1_DOOR_MAX);

  if (value >= area1_min) {
    this->parent_->set_area_range(AREA1_DOOR_MAX, value);
    ESP_LOGD(TAG, "Set area 1 max range to %.1f", this->parent_->get_area_range(AREA1_DOOR_MAX));

    if (this->parent_->joint_enable_door()) {
      this->publish_state(value);
    } else {
      ESP_LOGD(TAG, "Set area 1 max range failed");
      this->publish_state(area1_max);
    }
  } else {
    this->publish_state(area1_max);
    this->parent_->set_area_range(AREA1_DOOR_MAX, area1_max);
    // this->parent_->publish_text_("The maximum must be greater than the minimum.")
  }
}

// ===== 区域 2 =====
void Area2MinRangeNumber::control(float value) {
  float area2_min = this->parent_->get_area_range(AREA2_DOOR_MIN);
  float area2_max = this->parent_->get_area_range(AREA2_DOOR_MAX);

  if (value <= area2_max) {
    this->parent_->set_area_range(AREA2_DOOR_MIN, value);
    ESP_LOGD(TAG, "Set area 2 min range to %.1f", this->parent_->get_area_range(AREA2_DOOR_MIN));

    if (this->parent_->joint_enable_door()) {
      this->publish_state(value);
    } else {
      ESP_LOGD(TAG, "Set area 2 min range failed");
      this->publish_state(area2_min);
    }
  } else {
    this->publish_state(area2_min);
    this->parent_->set_area_range(AREA2_DOOR_MIN, area2_min);
    // this->parent_->publish_text_("The maximum must be greater than the minimum.")
  }
}

void Area2MaxRangeNumber::control(float value) {
  float area2_min = this->parent_->get_area_range(AREA2_DOOR_MIN);
  float area2_max = this->parent_->get_area_range(AREA2_DOOR_MAX);

  if (value >= area2_min) {
    ESP_LOGD(TAG, "Set area 2 max range to %.1f", value);
    this->parent_->set_area_range(AREA2_DOOR_MAX, value);

    if (this->parent_->joint_enable_door()) {
      this->publish_state(value);
    } else {
      ESP_LOGD(TAG, "Set area 2 max range failed");
      this->publish_state(area2_max);
    }
  } else {
    this->publish_state(area2_max);
    this->parent_->set_area_range(AREA2_DOOR_MAX, area2_max);
    // this->parent_->publish_text_("The maximum must be greater than the minimum.")
  }
}

// ===== 区域 3 =====
void Area3MinRangeNumber::control(float value) {
  float area3_min = this->parent_->get_area_range(AREA3_DOOR_MIN);
  float area3_max = this->parent_->get_area_range(AREA3_DOOR_MAX);

  if (value <= area3_max) {
    this->parent_->set_area_range(AREA3_DOOR_MIN, value);
    ESP_LOGD(TAG, "Set area 3 min range to %.1f", this->parent_->get_area_range(AREA3_DOOR_MIN));

    if (this->parent_->joint_enable_door()) {
      ESP_LOGD(TAG, "Set area 3 min range to %.1f", value);
      this->publish_state(value);
    } else {
      ESP_LOGD(TAG, "Set area 3 min range failed");
      this->publish_state(area3_min);
    }
  } else {
    this->publish_state(area3_min);
    this->parent_->set_area_range(AREA3_DOOR_MIN, area3_min);
    // this->parent_->publish_text_("The maximum must be greater than the minimum.")
  }
}

void Area3MaxRangeNumber::control(float value) {
  float area3_min = this->parent_->get_area_range(AREA3_DOOR_MIN);
  float area3_max = this->parent_->get_area_range(AREA3_DOOR_MAX);

  if (value >= area3_min) {
    this->parent_->set_area_range(AREA3_DOOR_MAX, value);
    ESP_LOGD(TAG, "Set area 3 max range to %.1f", this->parent_->get_area_range(AREA3_DOOR_MAX));

    if (this->parent_->joint_enable_door()) {
      this->publish_state(value);
    } else {
      ESP_LOGD(TAG, "Set area 3 max range failed");
      this->publish_state(area3_max);
    }
  } else {
    this->publish_state(area3_max);
    this->parent_->set_area_range(AREA3_DOOR_MAX, area3_max);
    // this->parent_->publish_text_("The maximum must be greater than the minimum.")
  }
}

void TargetDisappeardDelayTimeNumber::control(float value) {
  if (this->parent_->set_target_disappear_delay(value)) {
    ESP_LOGD(TAG, "Set target disappear delay time success");
    this->publish_state(value);
  } else {
    this->publish_state(NAN);
    ESP_LOGD(TAG, "Set target disappear delay time failed");
  }
}

}  // namespace dfrobot_c4002
}  // namespace esphome
