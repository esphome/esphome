#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/components/ir_rf_base/ir_rf_base.h"

namespace esphome::radio_frequency {

using ir_rf_base::CAPABILITY_RECEIVER;
using ir_rf_base::CAPABILITY_TRANSMITTER;
using ir_rf_base::IrRfCapability;

/// Modulation types supported by radio frequency implementations
enum RadioFrequencyModulation : uint8_t {
  RADIO_FREQUENCY_MODULATION_OOK = 0,  // On-Off Keying / Amplitude Shift Keying
  // Future: RADIO_FREQUENCY_MODULATION_FSK, RADIO_FREQUENCY_MODULATION_GFSK, etc.
};

class RadioFrequency;

/// RadioFrequencyCall - Builder pattern for transmitting radio frequency signals
class RadioFrequencyCall : public ir_rf_base::IrRfCall<RadioFrequencyCall, RadioFrequency> {
 public:
  explicit RadioFrequencyCall(RadioFrequency *parent) : IrRfCall(parent) {}

  /// Set the carrier frequency in Hz (e.g. 433920000 for 433.92 MHz)
  RadioFrequencyCall &set_frequency(uint32_t frequency_hz) {
    this->frequency_hz_ = frequency_hz;
    return *this;
  }
  /// Set the modulation type (defaults to OOK)
  RadioFrequencyCall &set_modulation(RadioFrequencyModulation modulation) {
    this->modulation_ = modulation;
    return *this;
  }
  /// Get the frequency in Hz
  const optional<uint32_t> &get_frequency() const { return this->frequency_hz_; }
  /// Get the modulation type
  RadioFrequencyModulation get_modulation() const { return this->modulation_; }

 protected:
  optional<uint32_t> frequency_hz_{};
  RadioFrequencyModulation modulation_{RADIO_FREQUENCY_MODULATION_OOK};
};

/// RadioFrequencyTraits - Describes the capabilities of a radio frequency implementation
class RadioFrequencyTraits {
 public:
  /// Hardware-supported tunable frequency range in Hz.
  /// If min == max (and both non-zero): fixed-frequency hardware.
  /// If both 0: range unspecified.
  uint32_t get_frequency_min_hz() const { return this->frequency_min_hz_; }
  void set_frequency_min_hz(uint32_t freq) { this->frequency_min_hz_ = freq; }

  uint32_t get_frequency_max_hz() const { return this->frequency_max_hz_; }
  void set_frequency_max_hz(uint32_t freq) { this->frequency_max_hz_ = freq; }

  /// Convenience setter for fixed-frequency hardware (sets min == max).
  void set_fixed_frequency_hz(uint32_t freq) {
    this->frequency_min_hz_ = freq;
    this->frequency_max_hz_ = freq;
  }

  /// Bitmask of supported RadioFrequencyModulation values (bit N = modulation value N supported).
  uint32_t get_supported_modulations() const { return this->supported_modulations_; }
  void set_supported_modulations(uint32_t mask) { this->supported_modulations_ = mask; }
  void add_supported_modulation(RadioFrequencyModulation mod) {
    this->supported_modulations_ |= (1u << static_cast<uint8_t>(mod));
  }

 protected:
  uint32_t frequency_min_hz_{0};       // Minimum tunable frequency in Hz (0 = unspecified)
  uint32_t frequency_max_hz_{0};       // Maximum tunable frequency in Hz (0 = unspecified)
  uint32_t supported_modulations_{0};  // Bitmask of supported RadioFrequencyModulation values
};

/// RadioFrequency - Base class for radio frequency implementations
class RadioFrequency : public ir_rf_base::IrRfEntity {
 public:
  RadioFrequency() = default;

  void dump_config() override;

  /// Get the traits for this radio frequency implementation
  RadioFrequencyTraits &get_traits() { return this->traits_; }
  const RadioFrequencyTraits &get_traits() const { return this->traits_; }

  /// Create a call object for transmitting
  RadioFrequencyCall make_call() { return RadioFrequencyCall(this); }

  /// Called when RF data is received (from RemoteReceiverListener)
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Add a callback to invoke when RF data is received
  template<typename F> void add_on_receive_callback(F &&callback) {
    this->receive_callback_.add(std::forward<F>(callback));
  }

  /// Add a callback to invoke when a transmit call is made on this entity.
  /// Fires before the platform-specific control() runs, with the call object
  /// (containing frequency, modulation, repeat count, etc.).  Used by the
  /// `on_control` YAML trigger so users can wire any RF front-end driver
  /// (CC1101, RFM69, custom) to react to per-call parameters.
  template<typename F> void add_on_control_callback(F &&callback) {
    this->control_callback_.add(std::forward<F>(callback));
  }

 protected:
  friend class ir_rf_base::IrRfCall<RadioFrequencyCall, RadioFrequency>;

  /// Fires the on_control hooks before the platform-specific control() runs
  void on_call_(const RadioFrequencyCall &call) { this->control_callback_.call(call); }
  /// Perform the actual transmission (called by RadioFrequencyCall::perform())
  /// Platforms must override this to implement hardware-specific transmission.
  /// Returns false if nothing was transmitted.
  virtual bool control(const RadioFrequencyCall &call) = 0;

  // Traits describing capabilities
  RadioFrequencyTraits traits_;

  // Callback manager for receive events (lazy: saves memory when no callbacks registered)
  LazyCallbackManager<void(remote_base::RemoteReceiveData)> receive_callback_;
  // Callback manager for on_control trigger (lazy: same memory savings)
  LazyCallbackManager<void(const RadioFrequencyCall &)> control_callback_;
};

}  // namespace esphome::radio_frequency
