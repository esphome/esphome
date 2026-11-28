#pragma once

#include "esphome/core/defines.h"
#include "zigbee_attribute_base_esp32.h"

#if defined(USE_ESP32) && defined(USE_ZIGBEE_COVER)

#include "esphome/components/cover/cover.h"

namespace esphome::zigbee {

/// Window Covering server cluster bound to a cover; owns the CurrentPositionLiftPercentage attribute.
class ZigbeeCover final : public ZigbeeAttributeBase {
 public:
  ZigbeeCover(ZigbeeComponent *parent, uint8_t endpoint_id, cover::Cover *cover, uint8_t covering_type)
      : ZigbeeAttributeBase(parent, endpoint_id, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER,
                            EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, EZB_ZCL_ATTR_TYPE_UINT8),
        covering_type_(covering_type),
        cover_(cover) {}
  void setup() override;
  void on_value(ezb_zcl_attribute_t attribute) override {}
  /// Runs on the Zigbee task. Without position support only 0 (open) and 100 (closed) can be reached.
  bool accepts_lift_percentage(uint8_t percentage) const {
    return percentage <= 100 && (this->supports_position_ || percentage % 100 == 0);
  }
  /// Runs on the Zigbee task.
  bool supports_stop() const { return this->supports_stop_; }
  void on_command(uint8_t cmd_id, uint8_t percentage);
  /// Runs on the Zigbee task. The SDK stores a GoToLiftPercentage target before the handler can reject it.
  void restore_lift_percentage();

  // After cover setup (initial position), before zigbee (WIFI)
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

 protected:
  uint8_t lift_percentage_() const;

  // Declared first so they fit in the base class tail padding
  bool supports_position_{false};
  bool supports_stop_{false};
  uint8_t value_{0};
  uint8_t covering_type_;
  cover::Cover *cover_;
};

}  // namespace esphome::zigbee

#endif
