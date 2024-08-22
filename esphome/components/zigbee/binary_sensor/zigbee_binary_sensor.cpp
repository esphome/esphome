#include "zigbee_binary_sensor.h"
#ifdef USE_ZIGBEE
#include "esphome/core/log.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_error_to_string.h>
}
namespace esphome {
namespace zigbee {

static const char *const TAG = "zigbee.binary_sensor";

void ZigbeeBinarySensor::setup() {
  add_on_state_callback([this](bool state) {
    if (state) {
      cluster_attributes_->present_value = 1;
    } else {
      cluster_attributes_->present_value = 0;
    }
    ESP_LOGD(TAG, "set attribute ep: %d, present_value %d", ep_, cluster_attributes_->present_value);
    ZB_ZCL_SET_ATTRIBUTE(ep_, ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &cluster_attributes_->present_value, ZB_FALSE);
    this->parent_->flush();
  });

  if (!this->publish_initial_state_)
    return;

  if (this->f_ != nullptr) {
    this->publish_initial_state(this->f_().value_or(false));
  } else {
    this->publish_initial_state(false);
  }
}

void ZigbeeBinarySensor::loop() {
  if (this->f_ == nullptr)
    return;

  auto s = this->f_();
  if (s.has_value()) {
    this->publish_state(*s);
  }
}

void ZigbeeBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Zigbee Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  EP: %d", ep_);
}

void ZigbeeBinarySensor::set_parent(Zigbee *parent) { this->parent_ = parent; }

}  // namespace zigbee
}  // namespace esphome
#endif
