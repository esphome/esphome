#pragma once

#include "esphome/components/openthread/openthread.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#ifdef USE_OPENTHREAD

namespace esphome::openthread_signal {

using esphome::openthread::InstanceLock;

class OpenThreadInstancePollingComponent : public PollingComponent {
 public:
  void update() override {
    auto lock = InstanceLock::try_acquire(10);
    if (!lock) {
      return;
    }

    this->update_instance(lock.get_instance());
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  virtual void update_instance(otInstance *instance) = 0;
};

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

class OpenThreadParentLqiInSensor final : public ParentOpenThreadSensor, public sensor::Sensor {
 public:
  void update_parent_info(otRouterInfo *parent_info) override {
    this->publish_state(parent_info->mLinkQualityIn);
  }
  void dump_config() override;
};

class OpenThreadParentLqiOutSensor final : public ParentOpenThreadSensor, public sensor::Sensor {
 public:
  void update_parent_info(otRouterInfo *parent_info) override {
    this->publish_state(parent_info->mLinkQualityOut);
  }
  void dump_config() override;
};

class OpenThreadParentPathCostSensor final : public ParentOpenThreadSensor, public sensor::Sensor {
 public:
  void update_parent_info(otRouterInfo *parent_info) override {
    this->publish_state(parent_info->mPathCost);
  }
  void dump_config() override;
};

class OpenThreadRssiSensor : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    int8_t rssi = otPlatRadioGetRssi(instance);
    if (rssi != OT_RADIO_RSSI_INVALID) {
      this->publish_state(rssi);
    }
  }
  void dump_config() override;
};

}  // namespace esphome::openthread_signal
#endif
