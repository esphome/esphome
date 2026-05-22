#include "zigbee_number_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_NUMBER)
#include "esphome/core/log.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_error_to_string.h>
}
namespace esphome::zigbee {

static const char *const TAG = "zigbee.number";

void ZigbeeNumber::setup() {
  this->parent_->add_callback(this->endpoint_, [this](zb_bufid_t bufid) { this->zcl_device_cb_(bufid); });
  this->number_->add_on_state_callback([this](float state) {
    this->cluster_attributes_->present_value = state;
    ESP_LOGD(TAG, "Set attribute endpoint: %d, present_value %f", this->endpoint_,
             this->cluster_attributes_->present_value);
    ZB_ZCL_SET_ATTRIBUTE(this->endpoint_, ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                         ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, (zb_uint8_t *) &cluster_attributes_->present_value,
                         ZB_FALSE);
    this->parent_->force_report();
  });
}

void ZigbeeNumber::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee Number\n"
                "  Endpoint: %d, present_value %f",
                this->endpoint_, this->cluster_attributes_->present_value);
}

void ZigbeeNumber::zcl_device_cb_(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;

  switch (device_cb_id) {
    /* ZCL set attribute value */
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (cluster_id == ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT) {
        ESP_LOGI(TAG, "Analog output attribute setting");
        if (attr_id == ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID) {
          float value =
              *reinterpret_cast<const float *>(&p_device_cb_param->cb_param.set_attr_value_param.values.data32);
          this->defer([this, value]() {
            this->cluster_attributes_->present_value = value;
            auto call = this->number_->make_call();
            call.set_value(value);
            call.perform();
          });
        }
      } else {
        /* other clusters attribute handled here */
        ESP_LOGI(TAG, "Unhandled cluster attribute id: %d", cluster_id);
        p_device_cb_param->status = RET_NOT_IMPLEMENTED;
      }
      break;
    default:
      p_device_cb_param->status = RET_NOT_IMPLEMENTED;
      break;
  }

  ESP_LOGD(TAG, "%s status: %hd", __func__, p_device_cb_param->status);
}

const zb_uint8_t ZB_ZCL_ANALOG_OUTPUT_STATUS_FLAG_MAX_VALUE = 0x0F;

static zb_ret_t check_value_analog_server(zb_uint16_t attr_id, zb_uint8_t endpoint,
                                          zb_uint8_t *value) {  // NOLINT(readability-non-const-parameter)
  zb_ret_t ret = RET_OK;
  ZVUNUSED(endpoint);

  switch (attr_id) {
    case ZB_ZCL_ATTR_ANALOG_OUTPUT_OUT_OF_SERVICE_ID:
      ret = ZB_ZCL_CHECK_BOOL_VALUE(*value) ? RET_OK : RET_ERROR;
      break;
    case ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID:
      break;

    case ZB_ZCL_ATTR_ANALOG_OUTPUT_STATUS_FLAG_ID:
      if (*value > ZB_ZCL_ANALOG_OUTPUT_STATUS_FLAG_MAX_VALUE) {
        ret = RET_ERROR;
      }
      break;

    default:
      break;
  }

  return ret;
}

}  // namespace esphome::zigbee

void zb_zcl_analog_output_init_server() {
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                              esphome::zigbee::check_value_analog_server, (zb_zcl_cluster_write_attr_hook_t) NULL,
                              (zb_zcl_cluster_handler_t) NULL);
}

void zb_zcl_analog_output_init_client() {
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ZB_ZCL_CLUSTER_CLIENT_ROLE,
                              (zb_zcl_cluster_check_value_t) NULL, (zb_zcl_cluster_write_attr_hook_t) NULL,
                              (zb_zcl_cluster_handler_t) NULL);
}

#endif
