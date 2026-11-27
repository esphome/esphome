#pragma once

#include "esphome/core/defines.h"
#include "zigbee_attribute_base_esp32.h"

#if defined(USE_ESP32) && defined(USE_ZIGBEE) && defined(USE_NUMBER)

#include "esphome/components/number/number.h"

namespace esphome::zigbee {

class ZigbeeNumber final : public ZigbeeAttributeBase {
 public:
  ZigbeeNumber(ZigbeeComponent *parent, uint8_t endpoint_id, number::Number *number)
      : ZigbeeAttributeBase(parent, endpoint_id, EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER,
                            EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, EZB_ZCL_ATTR_TYPE_SINGLE),
        number_(number) {}
  void setup() override;
  void on_value(ezb_zcl_attribute_t attribute) override;

  // After number setup (traits), before zigbee (WIFI)
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

 protected:
  // Declared first so it fits in the base class tail padding
  bool synchronizing_from_zigbee_{false};
  number::Number *number_;
  float value_{NAN};
};

}  // namespace esphome::zigbee

#endif
