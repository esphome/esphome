#include "zigbee_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52)
#include "esphome/core/log.h"
#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>

extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_error_to_string.h>
}

namespace esphome::zigbee {

static const char *const TAG = "zigbee";

ZigbeeComponent *global_zigbee = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const uint8_t IEEE_ADDR_BUF_SIZE = 17;

void ZigbeeComponent::zboss_signal_handler_esphome(zb_bufid_t bufid) {
  zb_zdo_app_signal_hdr_t *sig_hndler = nullptr;
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
        on_join_();
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
    case ZB_BDB_SIGNAL_TC_REJOIN_DONE:
      ESP_LOGD(TAG, "ZB_BDB_SIGNAL_TC_REJOIN_DONE, status: %d", status);
      break;
    default:
      ESP_LOGD(TAG, "zboss_signal_handler sig: %d, status: %d", sig, status);
      break;
  }

  auto err = zigbee_default_signal_handler(bufid);
  if (err != RET_OK) {
    ESP_LOGE(TAG, "Zigbee_default_signal_handler ERROR %u [%s]", err, zb_error_to_string_get(err));
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
            on_join_();
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

void ZigbeeComponent::zcl_device_cb(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;
  auto endpoint = p_device_cb_param->endpoint;

  ESP_LOGI(TAG, "Zcl_device_cb %s id %hd, cluster_id %d, attr_id %d, endpoint: %d", __func__, device_cb_id, cluster_id,
           attr_id, endpoint);

  // endpoints are enumerated from 1
  if (global_zigbee->callbacks_.size() >= endpoint) {
    global_zigbee->callbacks_[endpoint - 1](bufid);
    return;
  }
  p_device_cb_param->status = RET_ERROR;
}

void ZigbeeComponent::on_join_() {
  this->defer([this]() {
    ESP_LOGD(TAG, "Joined the network");
    this->join_trigger_.trigger();
    this->join_cb_.call();
  });
}

#ifdef USE_ZIGBEE_WIPE_ON_BOOT
void ZigbeeComponent::erase_flash_(int area) {
  const struct flash_area *fap;
  flash_area_open(area, &fap);
  flash_area_erase(fap, 0, fap->fa_size);
  flash_area_close(fap);
}
#endif

void ZigbeeComponent::setup() {
  global_zigbee = this;
  auto err = settings_subsys_init();
  if (err) {
    ESP_LOGE(TAG, "Failed to initialize settings subsystem, err: %d", err);
    return;
  }

#ifdef USE_ZIGBEE_WIPE_ON_BOOT
  erase_flash_(FIXED_PARTITION_ID(ZBOSS_NVRAM));
  erase_flash_(FIXED_PARTITION_ID(ZBOSS_PRODUCT_CONFIG));
  erase_flash_(FIXED_PARTITION_ID(SETTINGS_STORAGE));
#endif

  ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);
  err = settings_load();
  if (err) {
    ESP_LOGE(TAG, "Cannot load settings, err: %d", err);
    return;
  }
  zigbee_enable();
}

void ZigbeeComponent::dump_config() {
  bool wipe = false;
#ifdef USE_ZIGBEE_WIPE_ON_BOOT
  wipe = true;
#endif
  ESP_LOGCONFIG(TAG,
                "Zigbee\n"
                "  Wipe on boot: %s",
                YESNO(wipe));
}

static void send_attribute_report(zb_bufid_t bufid, zb_uint16_t cmd_id) {
  ESP_LOGD(TAG, "Force zboss scheduler to wake and send attribute report");
  zb_buf_free(bufid);
}

void ZigbeeComponent::flush() { this->need_flush_ = true; }

void ZigbeeComponent::loop() {
  if (this->need_flush_) {
    this->need_flush_ = false;
    zb_buf_get_out_delayed_ext(send_attribute_report, 0, 0);
  }
}

void ZigbeeComponent::factory_reset() {
  ESP_LOGD(TAG, "Factory reset");
  ZB_SCHEDULE_APP_CALLBACK(zb_bdb_reset_via_local_action, 0);
}

}  // namespace esphome::zigbee

extern "C" void zboss_signal_handler(zb_uint8_t param) {
  esphome::zigbee::global_zigbee->zboss_signal_handler_esphome(param);
}
#endif
