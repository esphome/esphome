#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/hal.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/remote_receiver/remote_receiver.h"

#ifdef USE_API
#include "esphome/components/api/api_pb2.h"
#endif

#include <vector>

namespace esphome::infrared_proxy {

/// Feature flags for infrared proxy component availability
enum InfraredProxyFeature : uint32_t {
  FEATURE_INFRARED_PROXY_ENABLED = 1 << 0,
};

/// Capability flags for individual infrared proxy instances
enum InfraredProxyCapability : uint32_t {
  CAPABILITY_TRANSMITTER = 1 << 0,  // Can transmit signals
  CAPABILITY_RECEIVER = 1 << 1,     // Can receive signals
  CAPABILITY_RADIO_FREQ = 1 << 2,   // Supports RF if set, otherwise infrared (IR) signals
};

#ifdef USE_API
/// Get global feature flags for infrared proxy component (not instance-specific)
inline static uint32_t get_infrared_proxy_feature_flags() {
  return InfraredProxyFeature::FEATURE_INFRARED_PROXY_ENABLED;
}
#endif

class InfraredProxyComponent : public Component, public EntityBase, public remote_base::RemoteReceiverListener {
 public:
  InfraredProxyComponent() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_receiver(remote_receiver::RemoteReceiverComponent *receiver) { this->receiver_ = receiver; }

  bool has_transmitter() const { return this->transmitter_ != nullptr; }
  bool has_receiver() const { return this->receiver_ != nullptr; }

#ifdef USE_API
  /// Get capability flags for this infrared proxy instance
  uint32_t get_capability_flags() const;

  /// Transmit IR data based on timing parameters and data bytes
  void transmit(const api::InfraredProxyTransmitRequest &msg);

  /// Called when IR data is received - implements RemoteReceiverListener interface
  bool on_receive(remote_base::RemoteReceiveData data) override;
#endif

 protected:
  // Underlying hardware components
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  remote_receiver::RemoteReceiverComponent *receiver_{nullptr};

#ifdef USE_API
  /// Encode data bytes into raw timings based on timing parameters
  void encode_data_(const api::InfraredProxyTimingParams &timing, const std::vector<uint8_t> &data,
                    remote_base::RemoteTransmitData *transmit_data);
#endif
};

}  // namespace esphome::infrared_proxy
