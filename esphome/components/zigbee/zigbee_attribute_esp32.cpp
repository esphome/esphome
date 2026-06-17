#include "zigbee_attribute_esp32.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

namespace esphome::zigbee {

static const char *const TAG = "zigbee.attribute";

void ZigbeeAttribute::set_attr_() {
  if (!this->zb_->is_connected()) {
    return;
  }
  if (esp_zb_lock_acquire(10 / portTICK_PERIOD_MS)) {
    esp_zb_zcl_status_t state = esp_zb_zcl_set_attribute_val(this->endpoint_id_, this->cluster_id_, this->role_,
                                                             this->attr_id_, this->value_p_, false);
    if (this->force_report_) {
      this->report_(true);
    }
    this->set_attr_requested_ = false;
    // Check for error
    if (state != ESP_ZB_ZCL_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Setting attribute failed, ZCL status: %u", static_cast<unsigned>(state));
    }
    esp_zb_lock_release();
  }
}

void ZigbeeAttribute::report_(bool has_lock) {
  if (!this->zb_->is_connected()) {
    return;
  }
  if (has_lock or esp_zb_lock_acquire(10 / portTICK_PERIOD_MS)) {
    esp_zb_zcl_report_attr_cmd_t cmd = {};
    cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    cmd.zcl_basic_cmd.dst_endpoint = 1;
    cmd.zcl_basic_cmd.src_endpoint = this->endpoint_id_;
    cmd.clusterID = this->cluster_id_;
    cmd.attributeID = this->attr_id_;

    esp_zb_zcl_report_attr_cmd_req(&cmd);
    if (!has_lock) {
      esp_zb_lock_release();
    }
  }
}

esp_zb_zcl_reporting_info_t ZigbeeAttribute::get_reporting_info() {
  esp_zb_zcl_reporting_info_t reporting_info = {};
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = this->endpoint_id_;
  reporting_info.cluster_id = this->cluster_id_;
  reporting_info.cluster_role = this->role_;
  reporting_info.attr_id = this->attr_id_;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.u.send_info.min_interval = 10;     /*!< Actual minimum reporting interval */
  reporting_info.u.send_info.max_interval = 0;      /*!< Actual maximum reporting interval */
  reporting_info.u.send_info.def_min_interval = 10; /*!< Default minimum reporting interval */
  reporting_info.u.send_info.def_max_interval = 0;  /*!< Default maximum reporting interval */
  reporting_info.u.send_info.delta.s16 = 0;         /*!< Actual reportable change */

  return reporting_info;
}

void ZigbeeAttribute::set_report(bool force) {
  this->report_enabled = true;
  this->force_report_ = force;
}

void ZigbeeAttribute::loop() {
  if (this->set_attr_requested_) {
    this->set_attr_();
  }

  if (!this->set_attr_requested_) {
    this->disable_loop();
  }
}

}  // namespace esphome::zigbee

#endif
#endif
