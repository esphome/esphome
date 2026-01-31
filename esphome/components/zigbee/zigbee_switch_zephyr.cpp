#include "zigbee_switch_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_SWITCH)
#include "esphome/core/log.h"
#include <zephyr/settings/settings.h>

extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_error_to_string.h>
}

namespace esphome::zigbee {

static const char *const TAG = "zigbee_on_off.switch";

void ZigbeeSwitch::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee Switch\n"
                "  Endpoint: %d, present_value %u",
                this->endpoint_, this->cluster_attributes_->present_value);
}

void ZigbeeSwitch::setup() {
  this->parent_->add_callback(this->endpoint_, [this](zb_bufid_t bufid) { this->zcl_device_cb_(bufid); });
  this->switch_->add_on_state_callback([this](bool state) {
    this->cluster_attributes_->present_value = state ? ZB_TRUE : ZB_FALSE;
    ESP_LOGD(TAG, "Set attribute endpoint: %d, present_value %d", this->endpoint_,
             this->cluster_attributes_->present_value);
    ZB_ZCL_SET_ATTRIBUTE(this->endpoint_, ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID, &this->cluster_attributes_->present_value,
                         ZB_FALSE);
    this->parent_->flush();
  });
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
        ESP_LOGI(TAG, "Binary output attribute setting to %hd", value);
        if (attr_id == ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID) {
          this->defer([this, value]() {
            this->cluster_attributes_->present_value = value ? ZB_TRUE : ZB_FALSE;
            this->switch_->publish_state(value);
          });
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

const zb_uint8_t ZB_ZCL_BINARY_OUTPUT_STATUS_FLAG_MAX_VALUE = 0x0F;

static zb_ret_t check_value_binary_output_server(zb_uint16_t attr_id, zb_uint8_t endpoint,
                                                 zb_uint8_t *value) {  // NOLINT(readability-non-const-parameter)
  zb_ret_t ret = RET_OK;
  ZVUNUSED(endpoint);

  switch (attr_id) {
    case ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID:
    case ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID:
      ret = ZB_ZCL_CHECK_BOOL_VALUE(*value) ? RET_OK : RET_ERROR;
      break;

    case ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAG_ID:
      if (*value > ZB_ZCL_BINARY_OUTPUT_STATUS_FLAG_MAX_VALUE) {
        ret = RET_ERROR;
      }
      break;

    default:
      break;
  }

  return ret;
}

}  // namespace esphome::zigbee

void zb_zcl_binary_output_init_server() {
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                              esphome::zigbee::check_value_binary_output_server,
                              (zb_zcl_cluster_write_attr_hook_t) NULL, (zb_zcl_cluster_handler_t) NULL);
}

void zb_zcl_binary_output_init_client() {
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, ZB_ZCL_CLUSTER_CLIENT_ROLE,
                              (zb_zcl_cluster_check_value_t) NULL, (zb_zcl_cluster_write_attr_hook_t) NULL,
                              (zb_zcl_cluster_handler_t) NULL);
}

#endif
