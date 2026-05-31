#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/remote_base/remote_base.h"

#include <vector>

namespace esphome::radio_frequency {

/// Capability flags for individual radio frequency instances
enum RadioFrequencyCapability : uint32_t {
  CAPABILITY_TRANSMITTER = 1 << 0,  // Can transmit signals
  CAPABILITY_RECEIVER = 1 << 1,     // Can receive signals
};

/// Modulation types supported by radio frequency implementations
enum RadioFrequencyModulation : uint8_t {
  RADIO_FREQUENCY_MODULATION_OOK = 0,  // On-Off Keying / Amplitude Shift Keying
  // Future: RADIO_FREQUENCY_MODULATION_FSK, RADIO_FREQUENCY_MODULATION_GFSK, etc.
};

/// Forward declarations
class RadioFrequency;

/// RadioFrequencyCall - Builder pattern for transmitting radio frequency signals
class RadioFrequencyCall {
 public:
  explicit RadioFrequencyCall(RadioFrequency *parent) : parent_(parent) {}

  /// Set the carrier frequency in Hz (e.g. 433920000 for 433.92 MHz)
  RadioFrequencyCall &set_frequency(uint32_t frequency_hz);

  /// Set the modulation type (defaults to OOK)
  RadioFrequencyCall &set_modulation(RadioFrequencyModulation modulation);

  // ===== Raw Timings Methods =====
  // All set_raw_timings_* methods store pointers/references to external data.
  // The referenced data must remain valid until perform() completes.
  // Safe pattern:   call.set_raw_timings_xxx(data); call.perform();  // synchronous
  // Unsafe pattern: call.set_raw_timings_xxx(data); defer([call]() { call.perform(); });  // data may be gone!

  /// Set the raw timings from a vector (positive = mark, negative = space)
  /// @note Lifetime: Stores a pointer to the vector. The vector must outlive perform().
  /// @note Usage: Primarily for lambdas/automations where the vector is in scope.
  RadioFrequencyCall &set_raw_timings(const std::vector<int32_t> &timings);

  /// Set the raw timings from base64url-encoded little-endian int32 data
  /// @note Lifetime: Stores a pointer to the string. The string must outlive perform().
  /// @note Usage: For web_server - base64url is fully URL-safe (uses '-' and '_').
  /// @note Decoding happens at perform() time, directly into the transmit buffer.
  RadioFrequencyCall &set_raw_timings_base64url(const std::string &base64url);

  /// Set the raw timings from packed protobuf sint32 data (zigzag + varint encoded)
  /// @note Lifetime: Stores a pointer to the buffer. The buffer must outlive perform().
  /// @note Usage: For API component where data comes directly from the protobuf message.
  RadioFrequencyCall &set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count);

  /// Set the number of times to repeat transmission (1 = transmit once, 2 = transmit twice, etc.)
  RadioFrequencyCall &set_repeat_count(uint32_t count);

  /// Perform the transmission
  void perform();

  /// Get the frequency in Hz
  const optional<uint32_t> &get_frequency() const { return this->frequency_hz_; }
  /// Get the modulation type
  RadioFrequencyModulation get_modulation() const { return this->modulation_; }
  /// Get the raw timings (only valid if set via set_raw_timings)
  const std::vector<int32_t> &get_raw_timings() const { return *this->raw_timings_; }
  /// Check if raw timings have been set (any format)
  bool has_raw_timings() const {
    return this->raw_timings_ != nullptr || this->packed_data_ != nullptr || this->base64url_ptr_ != nullptr;
  }
  /// Check if using packed data format
  bool is_packed() const { return this->packed_data_ != nullptr; }
  /// Check if using base64url data format
  bool is_base64url() const { return this->base64url_ptr_ != nullptr; }
  /// Get the base64url data string
  const std::string &get_base64url_data() const { return *this->base64url_ptr_; }
  /// Get packed data (only valid if set via set_raw_timings_packed)
  const uint8_t *get_packed_data() const { return this->packed_data_; }
  uint16_t get_packed_length() const { return this->packed_length_; }
  uint16_t get_packed_count() const { return this->packed_count_; }
  /// Get the repeat count
  uint32_t get_repeat_count() const { return this->repeat_count_; }

 protected:
  optional<uint32_t> frequency_hz_{};
  uint32_t repeat_count_{1};
  RadioFrequency *parent_;
  // Pointer to vector-based timings (caller-owned, must outlive perform())
  const std::vector<int32_t> *raw_timings_{nullptr};
  // Pointer to base64url-encoded string (caller-owned, must outlive perform())
  const std::string *base64url_ptr_{nullptr};
  // Pointer to packed protobuf buffer (caller-owned, must outlive perform())
  const uint8_t *packed_data_{nullptr};
  uint16_t packed_length_{0};
  uint16_t packed_count_{0};
  RadioFrequencyModulation modulation_{RADIO_FREQUENCY_MODULATION_OOK};
};

/// RadioFrequencyTraits - Describes the capabilities of a radio frequency implementation
class RadioFrequencyTraits {
 public:
  bool get_supports_transmitter() const { return this->supports_transmitter_; }
  void set_supports_transmitter(bool supports) { this->supports_transmitter_ = supports; }

  bool get_supports_receiver() const { return this->supports_receiver_; }
  void set_supports_receiver(bool supports) { this->supports_receiver_ = supports; }

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
  bool supports_transmitter_{false};
  bool supports_receiver_{false};
};

/// RadioFrequency - Base class for radio frequency implementations
class RadioFrequency : public Component, public EntityBase, public remote_base::RemoteReceiverListener {
 public:
  RadioFrequency() = default;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// Get the traits for this radio frequency implementation
  RadioFrequencyTraits &get_traits() { return this->traits_; }
  const RadioFrequencyTraits &get_traits() const { return this->traits_; }

  /// Create a call object for transmitting
  RadioFrequencyCall make_call();

  /// Get capability flags for this radio frequency instance
  uint32_t get_capability_flags() const;

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
  friend class RadioFrequencyCall;

  /// Perform the actual transmission (called by RadioFrequencyCall::perform())
  /// Platforms must override this to implement hardware-specific transmission.
  virtual void control(const RadioFrequencyCall &call) = 0;

  // Traits describing capabilities
  RadioFrequencyTraits traits_;

  // Callback manager for receive events (lazy: saves memory when no callbacks registered)
  LazyCallbackManager<void(remote_base::RemoteReceiveData)> receive_callback_;
  // Callback manager for on_control trigger (lazy: same memory savings)
  LazyCallbackManager<void(const RadioFrequencyCall &)> control_callback_;
};

}  // namespace esphome::radio_frequency
