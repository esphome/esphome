#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "esp_zigbee.h"
#include "zigbee_esp32.h"

namespace esphome::zigbee {

enum ZigbeeReportT {
  ZIGBEE_REPORT_DEFAULT,
  ZIGBEE_REPORT_COORDINATOR,
  ZIGBEE_REPORT_ENABLE,
  ZIGBEE_REPORT_FORCE,
};

class ZigbeeEntityBase : public Component, public ZigbeeAttributeInterface {
 public:
  void loop() override;
  void set_report(ZigbeeReportT report);
  void report() override;

  bool report_enabled = false;

 protected:
  ZigbeeEntityBase(ZigbeeComponent *parent, uint8_t endpoint_id) : zb_(parent), endpoint_id_(endpoint_id) {}
  /// Handle pending attribute writes and clear set_attr_requested_ when complete; leave it set to retry.
  virtual void set_attr_internal() = 0;
  /// Handle pending reports and clear report_requested_ when complete; leave it set to retry.
  /// Acquire/release the Zigbee lock only when has_lock is false; otherwise the caller owns it.
  virtual void report_internal(bool has_lock) = 0;
  ZigbeeComponent *zb_;
  uint8_t endpoint_id_;
  bool set_attr_requested_{false};
  bool report_requested_{false};
  bool force_report_{false};
};

}  // namespace esphome::zigbee

#endif
#endif
