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

namespace esphome::infrared {

/// Feature flags for infrared component availability
enum InfraredFeature : uint32_t {
  FEATURE_INFRARED_ENABLED = 1 << 0,
};

/// Capability flags for individual infrared instances
enum InfraredCapability : uint32_t {
  CAPABILITY_TRANSMITTER = 1 << 0,  // Can transmit signals
  CAPABILITY_RECEIVER = 1 << 1,     // Can receive signals
};

#ifdef USE_API
/// Get global feature flags for infrared component (not instance-specific)
inline uint32_t get_infrared_feature_flags() { return InfraredFeature::FEATURE_INFRARED_ENABLED; }
#endif

/// Forward declarations
class Infrared;

/// InfraredCall - Builder pattern for transmitting infrared signals
class InfraredCall {
 public:
  explicit InfraredCall(Infrared *parent) : parent_(parent) {}

  /// Set the carrier frequency in Hz
  InfraredCall &set_carrier_frequency(uint32_t frequency);
  /// Set the raw timings (positive = mark, negative = space)
  InfraredCall &set_raw_timings(const std::vector<int32_t> &timings);
  /// Set the number of times to repeat transmission (1 = transmit once, 2 = transmit twice, etc.)
  InfraredCall &set_repeat_count(uint32_t count);

  /// Perform the transmission
  void perform();

  /// Get the carrier frequency
  const optional<uint32_t> &get_carrier_frequency() const { return this->carrier_frequency_; }
  /// Get the raw timings
  const std::vector<int32_t> &get_raw_timings() const { return this->raw_timings_; }
  /// Get the repeat count
  uint32_t get_repeat_count() const { return this->repeat_count_; }

 protected:
  uint32_t repeat_count_{1};
  Infrared *parent_;
  optional<uint32_t> carrier_frequency_;
  std::vector<int32_t> raw_timings_;
};

/// InfraredTraits - Describes the capabilities of an infrared implementation
class InfraredTraits {
 public:
  bool get_supports_transmitter() const { return this->supports_transmitter_; }
  void set_supports_transmitter(bool supports) { this->supports_transmitter_ = supports; }

  bool get_supports_receiver() const { return this->supports_receiver_; }
  void set_supports_receiver(bool supports) { this->supports_receiver_ = supports; }

 protected:
  bool supports_transmitter_{false};
  bool supports_receiver_{false};
};

/// Infrared - Base class for infrared remote control implementations
class Infrared : public Component, public EntityBase {
 public:
  Infrared() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// Set the remote receiver component
  void set_receiver(remote_receiver::RemoteReceiverComponent *receiver) { this->receiver_ = receiver; }
  /// Set the remote transmitter component
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

  /// Check if this infrared has a transmitter configured
  bool has_transmitter() const { return this->transmitter_ != nullptr; }
  /// Check if this infrared has a receiver configured
  bool has_receiver() const { return this->receiver_ != nullptr; }

  /// Get the traits for this infrared implementation
  InfraredTraits &get_traits() { return this->traits_; }
  const InfraredTraits &get_traits() const { return this->traits_; }

  /// Create a call object for transmitting
  InfraredCall make_call();

#ifdef USE_API
  /// Get capability flags for this infrared instance
  uint32_t get_capability_flags() const;

  /// Transmit infrared data using raw timings array
  void transmit_raw_timings(const api::InfraredRFTransmitRawTimingsRequest &msg);
#endif

 protected:
  friend class InfraredCall;

  /// Perform the actual transmission (called by InfraredCall)
  virtual void control(const InfraredCall &call);

  // Underlying hardware components
  remote_receiver::RemoteReceiverComponent *receiver_{nullptr};
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};

  // Traits describing capabilities
  InfraredTraits traits_;
};

}  // namespace esphome::infrared
