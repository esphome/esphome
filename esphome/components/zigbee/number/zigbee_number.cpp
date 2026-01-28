#include "zigbee_number.h"
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

static const char *const TAG = "zigbee.number";

void ZigbeeNumber::setup() {
  this->parent_->add_callback(this->ep_, [this](zb_bufid_t bufid) { this->zcl_device_cb_(bufid); });
  add_on_state_callback([this](float state) {
    cluster_attributes_->present_value = state;
    ;
    ESP_LOGD(TAG, "set attribute ep: %d, present_value %f", ep_, state);
    ZB_ZCL_SET_ATTRIBUTE(ep_, ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, (zb_uint8_t *) &cluster_attributes_->present_value,
                         ZB_FALSE);
    this->parent_->flush();
  });

  if (this->f_ != nullptr) {
    this->publish_state(this->f_().value_or(0.0f));
  } else {
    this->publish_state(0.0f);
  }
}

void ZigbeeNumber::update() {
  if (this->f_ == nullptr)
    return;

  auto s = this->f_();
  if (s.has_value()) {
    this->publish_state(*s);
  }
}

void ZigbeeNumber::control(float value) { this->publish_state(value); }

void ZigbeeNumber::dump_config() {
  LOG_NUMBER("", "Zigbee Number", this);
  ESP_LOGCONFIG(TAG, "  EP: %d, present_value %f", ep_, cluster_attributes_->present_value);
}

void ZigbeeNumber::zcl_device_cb_(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;

  p_device_cb_param->status = RET_OK;

  switch (device_cb_id) {
    /* ZCL set attribute value */
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (cluster_id == ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT) {
        ESP_LOGI(TAG, "analog output attribute setting");
        if (attr_id == ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID) {
          uint32_t value = p_device_cb_param->cb_param.set_attr_value_param.values.data32;
          this->parent_->schedule([this, value]() { control(*reinterpret_cast<const float *>(&value)); });
        }
      } else {
        /* other clusters attribute handled here */
        ESP_LOGI(TAG, "Unhandled cluster attribute id: %d", cluster_id);
      }
      break;
    default:
      p_device_cb_param->status = RET_ERROR;
      break;
  }

  ESP_LOGD(TAG, "%s status: %hd", __func__, p_device_cb_param->status);
}

}  // namespace zigbee
}  // namespace esphome
#endif
