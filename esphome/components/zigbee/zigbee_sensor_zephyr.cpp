#include "zigbee_sensor_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_SENSOR)
#include "esphome/core/log.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_error_to_string.h>
}
namespace esphome::zigbee {

static const char *const TAG = "zigbee.sensor";

ZigbeeSensor::ZigbeeSensor(sensor::Sensor *sensor) : sensor_(sensor) {}

void ZigbeeSensor::setup() {
  this->sensor_->add_on_state_callback([this](float state) {
    this->cluster_attributes_->present_value = state;
    ESP_LOGD(TAG, "set attribute end point: %d, present_value %f", this->endpoint_, state);
    ZB_ZCL_SET_ATTRIBUTE(this->endpoint_, ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
                         (zb_uint8_t *) &this->cluster_attributes_->present_value, ZB_FALSE);
    this->parent_->flush();
  });
}

void ZigbeeSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee Sensor"
                "  end point: %d, present_value %f",
                this->endpoint_, this->cluster_attributes_->present_value);
}

}  // namespace esphome::zigbee
#endif
