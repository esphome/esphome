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
  if (esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    ezb_zcl_status_t state = ezb_zcl_set_attr_value(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_,
                                                    EZB_ZCL_STD_MANUF_CODE, this->value_p_, false);
    if (this->force_report_) {
      this->report_(true);
    }
    this->set_attr_requested_ = false;
    // Check for error
    if (state != EZB_ZCL_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Setting attribute failed, ZCL status: %u", static_cast<unsigned>(state));
    }
    esp_zigbee_lock_release();
  }
}

void ZigbeeAttribute::report_(bool has_lock) {
  if (!this->zb_->is_connected() || !this->report_enabled) {
    return;
  }
  if (has_lock or esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    ezb_zcl_report_attr_cmd_t cmd = {};
    cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.cmd_ctrl.fc.dis_default_rsp = 1;
    cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
    cmd.cmd_ctrl.dst_addr.u.short_addr = 0x0000;
    cmd.cmd_ctrl.dst_ep = 1;
    cmd.cmd_ctrl.src_ep = this->endpoint_id_;
    cmd.cmd_ctrl.cluster_id = this->cluster_id_;
    cmd.cmd_ctrl.fc.manuf_specific = 0;
    cmd.payload.attr_id = this->attr_id_;

    ezb_zcl_report_attr_cmd_req(&cmd);
    if (!has_lock) {
      esp_zigbee_lock_release();
    }
  }
}

void ZigbeeAttribute::set_report(ZigbeeReportT report) {
  this->report_enabled = true;
  if (report == ZigbeeReportT::ZIGBEE_REPORT_FORCE) {
    this->force_report_ = true;
  }
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
