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
  FEATURE_INFRARED_PROXY_SUPPORTS_GENERIC_PULSE_WIDTH = 1 << 1,
};

/// Capability flags for individual infrared proxy instances
enum InfraredProxyCapability : uint32_t {
  CAPABILITY_TRANSMITTER = 1 << 0,  // Can transmit signals
  CAPABILITY_RECEIVER = 1 << 1,     // Can receive signals
};

#ifdef USE_API
/// Get global feature flags for infrared proxy component (not instance-specific)
inline uint32_t get_infrared_proxy_feature_flags() {
  return InfraredProxyFeature::FEATURE_INFRARED_PROXY_ENABLED |
         InfraredProxyFeature::FEATURE_INFRARED_PROXY_SUPPORTS_GENERIC_PULSE_WIDTH;
}

/// Write JSON-formatted list of all supported infrared/RF protocols to output string
/// @param out Output string to write JSON array to (avoids intermediate allocations)
void get_infrared_proxy_supported_protocols(std::string &out);
#endif

class InfraredProxyComponent : public Component, public EntityBase, public remote_base::RemoteReceiverListener {
 public:
  InfraredProxyComponent() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_receiver(remote_receiver::RemoteReceiverComponent *receiver) { this->receiver_ = receiver; }
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

  /// Returns frequency in kHz (Hz / 1000), or 0 for infrared
  uint32_t get_frequency() const { return this->frequency_; }
  bool is_rf() const { return this->frequency_ > 0; }
  bool has_transmitter() const { return this->transmitter_ != nullptr; }
  bool has_receiver() const { return this->receiver_ != nullptr; }

#ifdef USE_API
  /// Get capability flags for this infrared proxy instance
  uint32_t get_capability_flags() const;

  /// Transmit IR/RF data using pulse width encoding parameters
  void transmit_pulse_width(const api::InfraredProxyTransmitPulseWidthRequest &msg);

  /// Transmit IR/RF data using JSON protocol specification
  void transmit_protocol(const api::InfraredProxyTransmitProtocolRequest &msg);

  /// Called when IR data is received - implements RemoteReceiverListener interface
  bool on_receive(remote_base::RemoteReceiveData data) override;
#endif

 protected:
#ifdef USE_API
  /// Encode data bytes into raw timings based on timing parameters
  void encode_data_(const api::InfraredProxyTimingParams &timing, const std::vector<uint8_t> &data,
                    remote_base::RemoteTransmitData *transmit_data);
#endif

  // Targeted RF frequency in kHz (Hz / 1000); 0 = infrared, non-zero = RF
  uint32_t frequency_{0};
  // Underlying hardware components
  remote_receiver::RemoteReceiverComponent *receiver_{nullptr};
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
};

}  // namespace esphome::infrared_proxy
