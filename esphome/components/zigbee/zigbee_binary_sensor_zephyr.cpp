#include "zigbee_binary_sensor_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_BINARY_SENSOR)
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

ZigbeeBinarySensor::ZigbeeBinarySensor(binary_sensor::BinarySensor *binary_sensor) : binary_sensor_(binary_sensor) {}

void ZigbeeBinarySensor::setup() {
  this->binary_sensor_->add_on_state_callback([this](bool state) {
    cluster_attributes_->present_value = state ? 1 : 0;
    ESP_LOGD(TAG, "set attribute ep: %d, present_value %d", ep_, cluster_attributes_->present_value);
    ZB_ZCL_SET_ATTRIBUTE(ep_, ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &cluster_attributes_->present_value, ZB_FALSE);
    this->parent_->flush();
  });
}

void ZigbeeBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Zigbee Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  EP: %d, present_value %u", ep_, cluster_attributes_->present_value);
}

}  // namespace zigbee
}  // namespace esphome
#endif
