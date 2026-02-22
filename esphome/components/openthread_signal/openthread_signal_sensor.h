#pragma once

#include <optional>
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

    this->update_instance(lock->get_instance());
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  virtual void update_instance(otInstance *instance) = 0;
};

class ParentLastRssiOpenThreadSignal final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    int8_t val;
    std::optional<int8_t> parent_last_rssi;
    if (otThreadGetParentLastRssi(instance, &val) == OT_ERROR_NONE) {
      parent_last_rssi = val;
    }
    if (this->last_rssi_ != parent_last_rssi) {  // Changed?
      this->last_rssi_ = parent_last_rssi;
      this->publish_state(parent_last_rssi.value_or(NAN));
    }
  }
  void dump_config() override;

 protected:
  std::optional<int8_t> last_rssi_;
};

class ParentAverageRssiOpenThreadSignal final : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 public:
  void update_instance(otInstance *instance) override {
    int8_t val;
    std::optional<int8_t> parent_last_rssi;
    if (otThreadGetParentAverageRssi(instance, &val) == OT_ERROR_NONE) {
      parent_last_rssi = val;
    }
    if (this->last_rssi_ != parent_last_rssi) {  // Changed?
      this->last_rssi_ = parent_last_rssi;
      this->publish_state(parent_last_rssi.value_or(NAN));
    }
  }
  void dump_config() override;

 protected:
  std::optional<int8_t> last_rssi_;
};

/** Nontemplate base class to implement otLinkGetCounters field sensors */
class BaseLinkCounterOpenThreadSignal : public OpenThreadInstancePollingComponent, public sensor::Sensor {
 protected:
  explicit BaseLinkCounterOpenThreadSignal(const char *info_name) : info_name_(info_name) {}

  void check_and_publish(otInstance *instance, uint32_t otMacCounters::*member_ptr) {
    // Fetching link counter delivers a pointer to internal instance data.
    // Also polling of total counter only expected in larger intervals.
    // So seems reasonable to fetch separately for sensor fields
    const otMacCounters *val = otLinkGetCounters(instance);
    std::optional<uint32_t> counter;
    if (val != nullptr) {  // Returned struct pointer valid?
      counter = val->*member_ptr;
    }
    if (this->last_ != counter) {  // Changed?
      this->last_ = counter;
      this->publish_state(counter.value_or(NAN));
    }
  }

  void dump_config() override;

 private:
  /** Description when logging sensor (dump_config), private ownership */
  const char *info_name_{nullptr};

 protected:
  /** Last seen value for change detection */
  std::optional<uint32_t> last_;
};

/** Template helper to implement otLinkGetCounters field sensors
 *
 * Shall avoid code duplication and bloat.
 *
 * @internal Move nontemplate aspects into base class to avoid bloat!
 *
 * @internal
 * Could be avoided if Python cg directly passes member e.g. via ctor.
 * But to avoid that Python knows too much about C struct internals.
 *
 * @tparam LinkCounterMemberPtr Member pointer to field in otMacCounters
 */
template<auto LinkCounterMemberPtr> class LinkCounterOpenThreadSignal final : public BaseLinkCounterOpenThreadSignal {
 public:
  explicit LinkCounterOpenThreadSignal(const char *info_name) : BaseLinkCounterOpenThreadSignal(info_name) {}

 protected:
  void update_instance(otInstance *instance) override { check_and_publish(instance, LinkCounterMemberPtr); }
};

// Link counters
// - Rx
using RxAddressFilteredCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxAddressFiltered>;
using RxErrFcsCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxErrFcs>;
using RxErrNoFrameCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxErrNoFrame>;
using RxErrOtherCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxErrOther>;
using RxErrSecCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxErrSec>;
using RxErrUnknownNeighborCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxErrUnknownNeighbor>;
using RxTotalCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mRxTotal>;
// - Tx
using TxErrAbortCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mTxErrAbort>;
using TxErrBusyChannelCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mTxErrBusyChannel>;
using TxErrCcaCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mTxErrCca>;
using TxRetryCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mTxRetry>;
using TxTotalCounterOpenThreadSignal = LinkCounterOpenThreadSignal<&otMacCounters::mTxTotal>;

}  // namespace esphome::openthread_signal
#endif
