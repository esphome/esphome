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

class ZigbeeDeviceBase : public Component, public ZigbeeAttributeInterface {
 public:
  void loop() override;
  void set_report(ZigbeeReportT report);
  void report() override;

  bool report_enabled = false;

 protected:
  ZigbeeDeviceBase(ZigbeeComponent *parent, uint8_t endpoint_id) : zb_(parent), endpoint_id_(endpoint_id) {}
  virtual void set_attr_internal() = 0;
  virtual void report_internal(bool has_lock) = 0;
  ZigbeeComponent *zb_;
  uint8_t endpoint_id_;
  bool set_attr_requested_{false};
  bool report_requested_{false};
  bool force_report_{false};
  template<typename T> T invalid_value_();
};

}  // namespace esphome::zigbee

#endif
#endif
