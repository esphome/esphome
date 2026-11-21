#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "esphome/core/defines.h"
#include "zigbee_attribute_base_esp32.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_NUMBER

#include "esp_zigbee.h"
#include "zigbee_esp32.h"
#include "esphome/components/number/number.h"

namespace esphome::zigbee {

class ZigbeeNumber final : public ZigbeeAttributeBase {
 public:
  ZigbeeNumber(ZigbeeComponent *parent, uint8_t endpoint_id)
      : ZigbeeAttributeBase(parent, endpoint_id, EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER,
                            EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, EZB_ZCL_ATTR_TYPE_SINGLE) {}
  void setup() override;
  void connect(number::Number *device) { this->device_ = device; }

  // need to run setup before the zigbee component and after number
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

 protected:
  number::Number *device_{nullptr};
  bool synchronizing_from_zigbee_{false};
};

}  // namespace esphome::zigbee

#endif
#endif
#endif
