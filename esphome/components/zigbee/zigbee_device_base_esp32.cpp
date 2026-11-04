#include "zigbee_device_base_esp32.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee.device_base");

void ZigbeeDeviceBase::set_report(ZigbeeReportT report) {
  if (report == ZigbeeReportT::ZIGBEE_REPORT_DEFAULT || report == ZigbeeReportT::ZIGBEE_REPORT_COORDINATOR) {
    return;
  }
  this->report_enabled = true;
  if (report == ZigbeeReportT::ZIGBEE_REPORT_FORCE) {
    this->force_report_ = true;
  }
  this->zb_->add_on_join_callback([this](bool) {
    if (this->report_requested_) {
      this->enable_loop();
    }
  });
}

void ZigbeeDeviceBase::report() {
  if (!this->report_enabled) {
    return;
  }
  this->report_requested_ = true;
  this->enable_loop();
}

void ZigbeeDeviceBase::loop() {
  if (this->set_attr_requested_) {
    this->set_attr_();
  }

  if (this->report_requested_) {
    this->report_(false);
  }

  if (!this->report_requested_ && !this->set_attr_requested_) {
    this->disable_loop();
  }
}

}  // namespace esphome::zigbee

#endif
#endif
