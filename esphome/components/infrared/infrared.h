#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/components/ir_rf_base/ir_rf_base.h"

namespace esphome::infrared {

using ir_rf_base::CAPABILITY_RECEIVER;
using ir_rf_base::CAPABILITY_TRANSMITTER;

class Infrared;

/// InfraredCall - Builder pattern for transmitting infrared signals
class InfraredCall : public ir_rf_base::IrRfCall<InfraredCall, Infrared> {
 public:
  explicit InfraredCall(Infrared *parent) : IrRfCall(parent) {}

  /// Set the carrier frequency in Hz
  InfraredCall &set_carrier_frequency(uint32_t frequency) {
    this->carrier_frequency_ = frequency;
    return *this;
  }
  /// Get the carrier frequency
  const optional<uint32_t> &get_carrier_frequency() const { return this->carrier_frequency_; }

 protected:
  optional<uint32_t> carrier_frequency_;
};

/// InfraredTraits - Describes the capabilities of an infrared implementation
class InfraredTraits {
 public:
  uint32_t get_receiver_frequency_hz() const { return this->receiver_frequency_hz_; }
  void set_receiver_frequency_hz(uint32_t freq) { this->receiver_frequency_hz_ = freq; }

 protected:
  uint32_t receiver_frequency_hz_{0};  // Demodulation frequency of the IR receiver in Hz (0 = unspecified)
};

/// Infrared - Base class for infrared remote control implementations
class Infrared : public ir_rf_base::IrRfEntity {
 public:
  Infrared() = default;

  void dump_config() override;

  /// Get the traits for this infrared implementation
  InfraredTraits &get_traits() { return this->traits_; }
  const InfraredTraits &get_traits() const { return this->traits_; }

  /// Create a call object for transmitting
  InfraredCall make_call() { return InfraredCall(this); }

 protected:
  friend class ir_rf_base::IrRfCall<InfraredCall, Infrared>;

  void on_call_(const InfraredCall &) {}
  /// Perform the actual transmission (called by InfraredCall); false only when no frame was handed
  /// to the transmitter, in which case no completion follows
  virtual bool control(const InfraredCall &call) {
    return this->transmit_raw_(call, call.get_carrier_frequency().value_or(0));
  }

  // Traits describing capabilities
  InfraredTraits traits_;
};

}  // namespace esphome::infrared
