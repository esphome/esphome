#pragma once

#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_SENSOR)

#include "openthread_info_text_sensor.h"
#include "esphome/components/sensor/sensor.h"

#include <openthread/link.h>
#include <openthread/thread.h>

namespace esphome::openthread_info {

class ParentOpenThreadSensor : public OpenThreadInstancePollingComponent {
 public:
  void update_instance(otInstance *instance) override {
    otRouterInfo parent_info;
    if (otThreadGetParentInfo(instance, &parent_info) != OT_ERROR_NONE) {
      return;
    }

    this->update_parent_info(&parent_info);
  }

 protected:
  virtual void update_parent_info(otRouterInfo *parent_info) = 0;
};

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
class ParentLinkQualityInOpenThreadInfo final : public ParentOpenThreadSensor, public sensor::Sensor {
 public:
  void update_parent_info(otRouterInfo *parent_info) override {
    this->publish_state(parent_info->mLinkQualityIn);
  }
  void dump_config() override;
};

// Outgoing link quality to parent (0-3). Only valid when device is a child.
class ParentLinkQualityOutOpenThreadInfo final : public ParentOpenThreadSensor, public sensor::Sensor {
 public:
  void update_parent_info(otRouterInfo *parent_info) override {
    this->publish_state(parent_info->mLinkQualityOut);
  }
  void dump_config() override;
};

// Network path cost to parent. Only valid when device is a child.
class ParentPathCostOpenThreadInfo final : public ParentOpenThreadSensor, public sensor::Sensor {
 public:
  void update_parent_info(otRouterInfo *parent_info) override {
    this->publish_state(parent_info->mPathCost);
  }
  void dump_config() override;
};

// Radio Last RSSI
class RssiOpenThreadInfo : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    int8_t rssi = otPlatRadioGetRssi(instance);
    if (rssi != OT_RADIO_RSSI_INVALID) {
      this->publish_state(rssi);
    }
  }
  void dump_config() override;
};

// --- MAC counters (otLinkGetCounters) — cumulative since boot ---

class TxTotalOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override { this->publish_state(otLinkGetCounters(instance)->mTxTotal); }
  void dump_config() override;
};

class TxRetriesOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override { this->publish_state(otLinkGetCounters(instance)->mTxRetry); }
  void dump_config() override;
};

class TxErrCcaOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override { this->publish_state(otLinkGetCounters(instance)->mTxErrCca); }
  void dump_config() override;
};

class TxErrAbortOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override { this->publish_state(otLinkGetCounters(instance)->mTxErrAbort); }
  void dump_config() override;
};

class RxTotalOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override { this->publish_state(otLinkGetCounters(instance)->mRxTotal); }
  void dump_config() override;
};

class RxErrFcsOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override { this->publish_state(otLinkGetCounters(instance)->mRxErrFcs); }
  void dump_config() override;
};

// --- MLE stability counters (otThreadGetMleCounters) — cumulative since boot ---

class AttachAttemptsOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    this->publish_state(otThreadGetMleCounters(instance)->mAttachAttempts);
  }
  void dump_config() override;
};

class ParentChangesOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    this->publish_state(otThreadGetMleCounters(instance)->mParentChanges);
  }
  void dump_config() override;
};

class PartitionIdChangesOpenThreadInfo final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    this->publish_state(otThreadGetMleCounters(instance)->mPartitionIdChanges);
  }
  void dump_config() override;
};

}  // namespace esphome::openthread_info
#endif
