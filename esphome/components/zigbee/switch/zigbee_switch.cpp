#include "zigbee_switch.h"
#ifdef USE_ZIGBEE
#include "esphome/core/log.h"
#include <zephyr/settings/settings.h>

extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_error_to_string.h>
}

namespace esphome {
namespace zigbee {

static const char *const TAG = "zigbee_on_off.switch";

void ZigbeeSwitch::dump_config() {
  LOG_SWITCH("", "Zigbee Switch", this);
  ESP_LOGCONFIG(TAG, "  EP: %d", ep_);
}

void ZigbeeSwitch::setup() {
  add_on_state_callback([this](bool state) {
    if (state) {
      cluster_attributes_->present_value = 1;
    } else {
      cluster_attributes_->present_value = 0;
    }
    ESP_LOGD(TAG, "set attribute ep: %d, present_value %d", ep_, cluster_attributes_->present_value);
    ZB_ZCL_SET_ATTRIBUTE(ep_, ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID, &cluster_attributes_->present_value, ZB_FALSE);
    this->parent_->flush();
  });

  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void ZigbeeSwitch::write_state(bool state) {
  if (state) {
    this->output_->turn_on();
  } else {
    this->output_->turn_off();
  }
  this->publish_state(state);
}

void ZigbeeSwitch::zcl_device_cb_(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;

  p_device_cb_param->status = RET_OK;

  switch (device_cb_id) {
    /* ZCL set attribute value */
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (cluster_id == ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT) {
        uint8_t value = p_device_cb_param->cb_param.set_attr_value_param.values.data8;
        ESP_LOGI(TAG, "binary output attribute setting to %hd", value);

        if (attr_id == ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID) {
          write_state((zb_bool_t) value);
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

void ZigbeeSwitch::set_parent(Zigbee *parent) {
  this->parent_ = parent;
  this->parent_->add_callback(this->ep_, [this](zb_bufid_t bufid) { this->zcl_device_cb_(bufid); });
}

}  // namespace zigbee
}  // namespace esphome
#endif
