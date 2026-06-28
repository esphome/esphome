#pragma once

#include "openthread_info_text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_OPENTHREAD

#include <openthread/thread.h>

namespace esphome::openthread_info {

// Parent RSSI (average) in dBm. Only valid when device is a child; skips publish otherwise.
class ParentAverageRssiOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    int8_t rssi;
    if (otThreadGetParentAverageRssi(instance, &rssi) != OT_ERROR_NONE) {
      return;
    }
    this->publish_state(rssi);
  }
  void dump_config() override;
};

// Parent RSSI (last received frame) in dBm. Only valid when device is a child.
class ParentLastRssiOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    int8_t rssi;
    if (otThreadGetParentLastRssi(instance, &rssi) != OT_ERROR_NONE) {
      return;
    }
    this->publish_state(rssi);
  }
  void dump_config() override;
};

// Incoming link quality from parent (0-3). Only valid when device is a child.
class ParentLinkQualityInOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    otRouterInfo parent_info;
    if (otThreadGetParentInfo(instance, &parent_info) != OT_ERROR_NONE) {
      return;
    }
    this->publish_state(parent_info.mLinkQualityIn);
  }
  void dump_config() override;
};

// Outgoing link quality to parent (0-3). Only valid when device is a child.
class ParentLinkQualityOutOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    otRouterInfo parent_info;
    if (otThreadGetParentInfo(instance, &parent_info) != OT_ERROR_NONE) {
      return;
    }
    this->publish_state(parent_info.mLinkQualityOut);
  }
  void dump_config() override;
};

}  // namespace esphome::openthread_info
#endif
