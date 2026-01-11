#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/components/remote_base/remote_base.h"

#include <vector>

namespace esphome::infrared {

/// Capability flags for individual infrared instances
enum InfraredCapability : uint32_t {
  CAPABILITY_TRANSMITTER = 1 << 0,  // Can transmit signals
  CAPABILITY_RECEIVER = 1 << 1,     // Can receive signals
};

/// Forward declarations
class Infrared;

/// InfraredCall - Builder pattern for transmitting infrared signals
class InfraredCall {
 public:
  explicit InfraredCall(Infrared *parent) : parent_(parent) {}

  /// Set the carrier frequency in Hz
  InfraredCall &set_carrier_frequency(uint32_t frequency);
  /// Set the raw timings (positive = mark, negative = space)
  /// Note: The timings vector must outlive the InfraredCall (zero-copy reference)
  InfraredCall &set_raw_timings(const std::vector<int32_t> &timings);
  /// Set the raw timings from packed protobuf sint32 data (zero-copy from wire)
  /// Note: The data must outlive the InfraredCall
  InfraredCall &set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count);
  /// Set the number of times to repeat transmission (1 = transmit once, 2 = transmit twice, etc.)
  InfraredCall &set_repeat_count(uint32_t count);

  /// Perform the transmission
  void perform();

  /// Get the carrier frequency
  const optional<uint32_t> &get_carrier_frequency() const { return this->carrier_frequency_; }
  /// Get the raw timings (only valid if set via set_raw_timings, not packed)
  const std::vector<int32_t> &get_raw_timings() const { return *this->raw_timings_; }
  /// Check if raw timings have been set (either vector or packed)
  bool has_raw_timings() const { return this->raw_timings_ != nullptr || this->packed_data_ != nullptr; }
  /// Check if using packed data format
  bool is_packed() const { return this->packed_data_ != nullptr; }
  /// Get packed data (only valid if set via set_raw_timings_packed)
  const uint8_t *get_packed_data() const { return this->packed_data_; }
  uint16_t get_packed_length() const { return this->packed_length_; }
  uint16_t get_packed_count() const { return this->packed_count_; }
  /// Get the repeat count
  uint32_t get_repeat_count() const { return this->repeat_count_; }

 protected:
  uint32_t repeat_count_{1};
  Infrared *parent_;
  optional<uint32_t> carrier_frequency_;
  // Vector-based timings (for lambdas/automations)
  const std::vector<int32_t> *raw_timings_{nullptr};
  // Packed protobuf timings (for API zero-copy)
  const uint8_t *packed_data_{nullptr};
  uint16_t packed_length_{0};
  uint16_t packed_count_{0};
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
class Infrared : public Component, public EntityBase, public remote_base::RemoteReceiverListener {
 public:
  Infrared() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// Set the remote receiver component
  void set_receiver(remote_base::RemoteReceiverBase *receiver) { this->receiver_ = receiver; }
  /// Set the remote transmitter component
  void set_transmitter(remote_base::RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }

  /// Check if this infrared has a transmitter configured
  bool has_transmitter() const { return this->transmitter_ != nullptr; }
  /// Check if this infrared has a receiver configured
  bool has_receiver() const { return this->receiver_ != nullptr; }

  /// Get the traits for this infrared implementation
  InfraredTraits &get_traits() { return this->traits_; }
  const InfraredTraits &get_traits() const { return this->traits_; }

  /// Create a call object for transmitting
  InfraredCall make_call();

  /// Get capability flags for this infrared instance
  uint32_t get_capability_flags() const;

  /// Called when IR data is received (from RemoteReceiverListener)
  bool on_receive(remote_base::RemoteReceiveData data) override;

 protected:
  friend class InfraredCall;

  /// Perform the actual transmission (called by InfraredCall)
  virtual void control(const InfraredCall &call);

  // Underlying hardware components
  remote_base::RemoteReceiverBase *receiver_{nullptr};
  remote_base::RemoteTransmitterBase *transmitter_{nullptr};

  // Traits describing capabilities
  InfraredTraits traits_;
};

}  // namespace esphome::infrared
