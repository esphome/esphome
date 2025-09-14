#include "zigbee_sensor.h"
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

static const char *const TAG = "zigbee.sensor";

void ZigbeeSensor::setup() {
  add_on_state_callback([this](float state) {
    cluster_attributes_->present_value = state;
    ;
    ESP_LOGD(TAG, "set attribute ep: %d, present_value %f", ep_, state);
    ZB_ZCL_SET_ATTRIBUTE(ep_, ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, (zb_uint8_t *) &cluster_attributes_->present_value,
                         ZB_FALSE);
    this->parent_->flush();
  });

  if (this->f_ != nullptr) {
    this->publish_state(this->f_().value_or(0.0f));
  } else {
    this->publish_state(0.0f);
  }
}

void ZigbeeSensor::update() {
  if (this->f_ == nullptr)
    return;

  auto s = this->f_();
  if (s.has_value()) {
    this->publish_state(*s);
  }
}

void ZigbeeSensor::dump_config() {
  LOG_SENSOR("", "Zigbee Sensor", this);
  ESP_LOGCONFIG(TAG, "  EP: %d, present_value %f", ep_, cluster_attributes_->present_value);
}

}  // namespace zigbee
}  // namespace esphome
#endif
