#include "zigbee_component.h"
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

static const char *const TAG = "zigbee";

Zigbee *global_zigbee = nullptr;

#define IEEE_ADDR_BUF_SIZE 17

void Zigbee::zboss_signal_handler_esphome(zb_bufid_t bufid) {
  zb_zdo_app_signal_hdr_t *sig_hndler = NULL;
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &sig_hndler);
  zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

  switch (sig) {
    case ZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_SKIP_STARTUP, status: %d", status);
      break;
    case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
      ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY, status: %d", status);
      break;
    case ZB_ZDO_SIGNAL_LEAVE:
      ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_LEAVE, status: %d", status);
      break;
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
      ESP_LOGD(TAG, "ZB_BDB_SIGNAL_DEVICE_REBOOT, status: %d", status);
      if (status == RET_OK) {
        // this is from wrong thread
        this->join_trigger_->trigger();
      }
      break;
    case ZB_BDB_SIGNAL_STEERING:
      break;
    case ZB_COMMON_SIGNAL_CAN_SLEEP:
      ESP_LOGV(TAG, "ZB_COMMON_SIGNAL_CAN_SLEEP, status: %d", status);
      break;
    case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      ESP_LOGD(TAG, "ZB_BDB_SIGNAL_DEVICE_FIRST_START, status: %d", status);
      break;
    case ZB_NLME_STATUS_INDICATION:
      ESP_LOGD(TAG, "ZB_NLME_STATUS_INDICATION, status: %d", status);
      break;
    default:
      ESP_LOGD(TAG, "zboss_signal_handler sig: %d, status: %d", sig, status);
      break;
  }

  auto err = zigbee_default_signal_handler(bufid);
  if (err != RET_OK) {
    ESP_LOGE(TAG, "zigbee_default_signal_handler ERROR %u [%s]", err, zb_error_to_string_get(err));
  }

  switch (sig) {
    case ZB_BDB_SIGNAL_STEERING:
      ESP_LOGD(TAG, "ZB_BDB_SIGNAL_STEERING, status: %d", status);
      if (status == RET_OK) {
        zb_ext_pan_id_t extended_pan_id;
        char ieee_addr_buf[IEEE_ADDR_BUF_SIZE] = {0};
        int addr_len;

        zb_get_extended_pan_id(extended_pan_id);
        addr_len = ieee_addr_to_str(ieee_addr_buf, sizeof(ieee_addr_buf), extended_pan_id);

        for (int i = 0; i < addr_len; ++i) {
          if (ieee_addr_buf[i] != '0') {
            // called from wrong thread
            this->join_trigger_->trigger();
            break;
          }
        }
      }
      break;
  }

  /* All callbacks should either reuse or free passed buffers.
   * If bufid == 0, the buffer is invalid (not passed).
   */
  if (bufid) {
    zb_buf_free(bufid);
  }
}

void Zigbee::zcl_device_cb(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;
  auto endpoint = p_device_cb_param->endpoint;

  ESP_LOGI(TAG, "zcl_device_cb %s id %hd, cluster_id %d, attr_id %d, endpoint: %d", __func__, device_cb_id, cluster_id,
           attr_id, endpoint);

  auto cb = global_zigbee->callbacks_.find(endpoint);
  if (cb != global_zigbee->callbacks_.end()) {
    cb->second(bufid);
    return;
  }
  p_device_cb_param->status = RET_ERROR;
}

void Zigbee::setup() {
  global_zigbee = this;
  auto err = settings_subsys_init();
  if (err) {
    ESP_LOGE(TAG, "Failed to initialize settings subsystem, err: %d", err);
    return;
  }

  /* Register callback for handling ZCL commands. */
  ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);

  /* Settings should be loaded after zcl_scenes_init */
  err = settings_load();
  if (err) {
    ESP_LOGE(TAG, "Cannot load settings, err: %d", err);
    return;
  }

  /* Start Zigbee default thread */
  zigbee_enable();
}

static void send_attribute_report(zb_bufid_t bufid, zb_uint16_t cmd_id) {
  ESP_LOGD(TAG, "force zboss scheduler to wake and send attribute report");
  zb_buf_free(bufid);
}

void Zigbee::flush() { need_flush_ = true; }

void Zigbee::loop() {
  if (need_flush_) {
    need_flush_ = false;
    zb_buf_get_out_delayed_ext(send_attribute_report, 0, 0);
  }
}

void Zigbee::factory_reset() {
  ESP_LOGD(TAG, "factory reset");
  ZB_SCHEDULE_APP_CALLBACK(zb_bdb_reset_via_local_action, 0);
}

}  // namespace zigbee
}  // namespace esphome

extern "C" void zboss_signal_handler(zb_bufid_t bufid) {
  esphome::zigbee::global_zigbee->zboss_signal_handler_esphome(bufid);
}
#endif
