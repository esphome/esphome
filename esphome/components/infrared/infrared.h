#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/components/remote_base/remote_base.h"

#include <vector>

#if defined(USE_API) && defined(USE_IR_RF)
namespace esphome::api {
class APIConnection;
}  // namespace esphome::api
#endif

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

  // ===== Raw Timings Methods =====
  // All set_raw_timings_* methods store pointers/references to external data.
  // The referenced data must remain valid until perform() completes.
  // Safe pattern:   call.set_raw_timings_xxx(data); call.perform();  // synchronous
  // Unsafe pattern: call.set_raw_timings_xxx(data); defer([call]() { call.perform(); });  // data may be gone!

  /// Set the raw timings from a vector (positive = mark, negative = space)
  /// @note Lifetime: Stores a pointer to the vector. The vector must outlive perform().
  /// @note Usage: Primarily for lambdas/automations where the vector is in scope.
  InfraredCall &set_raw_timings(const std::vector<int32_t> &timings);

  /// Set the raw timings from base64url-encoded little-endian int32 data
  /// @note Lifetime: Stores a pointer to the string. The string must outlive perform().
  /// @note Usage: For web_server - base64url is fully URL-safe (uses '-' and '_').
  /// @note Decoding happens at perform() time, directly into the transmit buffer.
  InfraredCall &set_raw_timings_base64url(const std::string &base64url);

  /// Set the raw timings from packed protobuf sint32 data (zigzag + varint encoded)
  /// @note Lifetime: Stores a pointer to the buffer. The buffer must outlive perform().
  /// @note Usage: For API component where data comes directly from the protobuf message.
  InfraredCall &set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count);

  /// Set the number of times to repeat transmission (1 = transmit once, 2 = transmit twice, etc.)
  InfraredCall &set_repeat_count(uint32_t count);

  /// Perform the transmission; returns true if a frame was handed to the transmitter
  bool perform();
#if defined(USE_API) && defined(USE_IR_RF)
  /// Reply to this API client once the frame has left the transmitter (API 1.18+)
  InfraredCall &set_api_connection(api::APIConnection *conn) {
    this->api_connection_ = conn;
    return *this;
  }
#endif

  /// Get the carrier frequency
  const optional<uint32_t> &get_carrier_frequency() const { return this->carrier_frequency_; }
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
  uint32_t repeat_count_{1};
  Infrared *parent_;
#if defined(USE_API) && defined(USE_IR_RF)
  api::APIConnection *api_connection_{nullptr};
#endif
  optional<uint32_t> carrier_frequency_;
  // Pointer to vector-based timings (caller-owned, must outlive perform())
  const std::vector<int32_t> *raw_timings_{nullptr};
  // Pointer to base64url-encoded string (caller-owned, must outlive perform())
  const std::string *base64url_ptr_{nullptr};
  // Pointer to packed protobuf buffer (caller-owned, must outlive perform())
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

  uint32_t get_receiver_frequency_hz() const { return this->receiver_frequency_hz_; }
  void set_receiver_frequency_hz(uint32_t freq) { this->receiver_frequency_hz_ = freq; }

 protected:
  bool supports_transmitter_{false};
  bool supports_receiver_{false};
  uint32_t receiver_frequency_hz_{0};  // Demodulation frequency of the IR receiver in Hz (0 = unspecified)
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
  InfraredCall make_call() { return InfraredCall(this); }

  /// Get capability flags for this infrared instance
  uint32_t get_capability_flags() const;

  /// Called when IR data is received (from RemoteReceiverListener)
  bool on_receive(remote_base::RemoteReceiveData data) override;

#if defined(USE_API) && defined(USE_IR_RF)
  void loop() override;
  /// The API server calls this when a client disconnects, so no reply goes to a stale pointer
  void on_api_connection_closed(api::APIConnection *conn);
#endif

 protected:
  friend class InfraredCall;

  /// Perform the actual transmission (called by InfraredCall); false if nothing was transmitted
  virtual bool control(const InfraredCall &call);
  /// Answers the API request waiting on this entity, if any
  void notify_transmit_complete_();
  uint32_t inflight_seq_{0};  // seq of the frame this entity submitted last

  // Underlying hardware components
  remote_base::RemoteReceiverBase *receiver_{nullptr};
  remote_base::RemoteTransmitterBase *transmitter_{nullptr};

#if defined(USE_API) && defined(USE_IR_RF)
  // One reply slot: a pacing client has at most one transmit outstanding, and a second request
  // from an unpaced client displaces the first. Retried and expired from loop(), which only
  // runs while a reply is pending.
  enum class ApiReply : uint8_t {
    API_REPLY_NONE,
    API_REPLY_WAITING,     // frame handed to the transmitter, completion not reported yet
    API_REPLY_OWED_OK,     // reply refused by a full TCP buffer; loop() retries it
    API_REPLY_OWED_FAILED  // same, for a transmit that did not start
  };
  void expect_api_reply_(api::APIConnection *conn);
  void finish_api_reply_(bool success);
  bool send_api_reply_();
  api::APIConnection *api_reply_connection_{nullptr};
  uint32_t api_reply_registered_ms_{0};
#endif

  // Traits describing capabilities
  InfraredTraits traits_;
#if defined(USE_API) && defined(USE_IR_RF)
  ApiReply api_reply_{ApiReply::API_REPLY_NONE};
#endif
};

}  // namespace esphome::infrared
