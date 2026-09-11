#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/remote_base/remote_base.h"

#include <string>
#include <vector>

#if defined(USE_API) && defined(USE_IR_RF)
namespace esphome::api {
class APIConnection;
}  // namespace esphome::api
#endif

namespace esphome::ir_rf_base {

/// Capability flags reported by infrared and radio frequency entities
enum IrRfCapability : uint32_t {
  CAPABILITY_TRANSMITTER = 1 << 0,  // Can transmit signals
  CAPABILITY_RECEIVER = 1 << 1,     // Can receive signals
};

/// Raw timings of a transmit call, in one of three caller-owned forms
class IrRfCallData {
 public:
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

#if defined(USE_API) && defined(USE_IR_RF)
  /// True for a frame an API client is waiting on
  bool wants_api_reply() const { return this->api_connection_ != nullptr; }
#endif

 protected:
  uint32_t repeat_count_{1};
#if defined(USE_API) && defined(USE_IR_RF)
  api::APIConnection *api_connection_{nullptr};
#endif
  // Pointer to vector-based timings (caller-owned, must outlive perform())
  const std::vector<int32_t> *raw_timings_{nullptr};
  // Pointer to base64url-encoded string (caller-owned, must outlive perform())
  const std::string *base64url_ptr_{nullptr};
  // Pointer to packed protobuf buffer (caller-owned, must outlive perform())
  const uint8_t *packed_data_{nullptr};
  uint16_t packed_length_{0};
  uint16_t packed_count_{0};
};

template<typename Call, typename Entity> class IrRfCall;

/// Everything an infrared or radio frequency entity does that does not depend on the medium:
/// the remote_base transport, forwarding received frames to the API, and answering the API
/// once a transmit it started has left the transmitter.
class IrRfEntity : public Component, public EntityBase, public remote_base::RemoteReceiverListener {
 public:
  /// Reports the configured transports, listens on the receiver and hooks the transmitter's
  /// completion; a platform with its own setup() calls it first
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// Set the remote receiver component
  void set_receiver(remote_base::RemoteReceiverBase *receiver) { this->receiver_ = receiver; }
  /// Set the remote transmitter component
  void set_transmitter(remote_base::RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }
  bool has_transmitter() const { return this->transmitter_ != nullptr; }
  bool has_receiver() const { return this->receiver_ != nullptr; }

  /// What the entity can do; platforms with their own hardware set these from their own
  /// setup(), remote_base ones get them from IrRfEntity::setup()
  bool get_supports_transmitter() const { return this->supports_transmitter_; }
  void set_supports_transmitter(bool supports) { this->supports_transmitter_ = supports; }
  bool get_supports_receiver() const { return this->supports_receiver_; }
  void set_supports_receiver(bool supports) { this->supports_receiver_ = supports; }
  /// Capability flags as reported to the API and web server
  uint32_t get_capability_flags() const {
    uint32_t flags = 0;
    if (this->supports_transmitter_)
      flags |= CAPABILITY_TRANSMITTER;
    if (this->supports_receiver_)
      flags |= CAPABILITY_RECEIVER;
    return flags;
  }

  /// Forwards a received frame to the API; never consumes it, so other listeners still run
  bool on_receive(remote_base::RemoteReceiveData data) override;
#ifdef USE_IR_RF_TRANSMIT_COMPLETE
  /// Called for every finished frame on every transmitter; answers the API request when the
  /// frame is the one this entity submitted for it, with sent as the outcome
  void on_transmit_complete(remote_base::RemoteTransmitterBase *transmitter, uint16_t seq, bool sent);
#endif

#if defined(USE_API) && defined(USE_IR_RF)
  void loop() override;
  /// The API server calls this when a client disconnects, so no reply goes to a stale pointer
  void on_api_connection_closed(api::APIConnection *conn);
#endif

 protected:
  template<typename, typename> friend class IrRfCall;

  /// Hands the call's timings to the transmitter; the default transmit path of both entity types
  bool transmit_raw_(const IrRfCallData &call, uint32_t carrier_frequency_hz);
#if defined(USE_API) && defined(USE_IR_RF)
  // One reply slot: a pacing client has at most one transmit outstanding, and a second request
  // from an unpaced client displaces the first. Retried and expired from loop(), which only
  // runs while a reply is pending; polling the expiry there costs nothing while idle, where a
  // scheduler timeout would allocate per frame.
  enum class ApiReply : uint8_t {
    API_REPLY_NONE,
    API_REPLY_WAITING,     // frame handed to the transmitter, completion not reported yet
    API_REPLY_OWED_OK,     // reply refused by a full TCP buffer; loop() retries it
    API_REPLY_OWED_FAILED  // same, for a transmit that did not start
  };
  void expect_api_reply_(api::APIConnection *conn);
  /// After control(): a false start is answered now, and the loop only runs once something is
  /// pending, since a blocking transmitter has already answered inside control()
  void settle_api_reply_(bool started) {
#ifdef USE_IR_RF_TRANSMIT_COMPLETE
    if (!started)
      this->finish_api_reply_(false);
#else
    // no transmitter in this build reports completion, so the hand-over is the answer
    this->finish_api_reply_(started);
#endif
    if (this->api_reply_ != ApiReply::API_REPLY_NONE)
      this->enable_loop();
  }
  void finish_api_reply_(bool success);
  bool send_api_reply_();
  void clear_api_reply_() {
    this->api_reply_ = ApiReply::API_REPLY_NONE;
    this->api_reply_connection_ = nullptr;
    this->disable_loop();
  }
  api::APIConnection *api_reply_connection_{nullptr};
#endif

  remote_base::RemoteReceiverBase *receiver_{nullptr};
  remote_base::RemoteTransmitterBase *transmitter_{nullptr};
#ifdef USE_IR_RF_TRANSMIT_COMPLETE
  uint16_t api_reply_deadline_{0};  // in 16 ms ticks: 30 s plus the frame's own air time, capped
  uint16_t inflight_seq_{0};        // seq of the API frame this entity submitted last
#endif
  // short members last, so the derived traits start on the next word without a gap
  bool supports_transmitter_{false};
  bool supports_receiver_{false};
#if defined(USE_API) && defined(USE_IR_RF)
  ApiReply api_reply_{ApiReply::API_REPLY_NONE};
#endif
};

/// Builder for a transmit; Call is the concrete call type and Entity its entity, so the fluent
/// setters return the concrete type and control() sees the medium specific fields
template<typename Call, typename Entity> class IrRfCall : public IrRfCallData {
  // only the concrete call may construct the CRTP base
  friend Call;
  explicit IrRfCall(Entity *parent) : parent_(parent) {}

 public:
  // ===== Raw Timings Methods =====
  // All set_raw_timings_* methods store pointers/references to external data.
  // The referenced data must remain valid until perform() completes.
  // Safe pattern:   call.set_raw_timings_xxx(data); call.perform();  // synchronous
  // Unsafe pattern: call.set_raw_timings_xxx(data); defer([call]() { call.perform(); });  // data may be gone!

  /// Set the raw timings from a vector (positive = mark, negative = space)
  /// @note Lifetime: Stores a pointer to the vector. The vector must outlive perform().
  /// @note Usage: Primarily for lambdas/automations where the vector is in scope.
  Call &set_raw_timings(const std::vector<int32_t> &timings) {
    this->raw_timings_ = &timings;
    this->packed_data_ = nullptr;
    this->base64url_ptr_ = nullptr;
    return this->self_();
  }

  /// Set the raw timings from base64url-encoded little-endian int32 data
  /// @note Lifetime: Stores a pointer to the string. The string must outlive perform().
  /// @note Usage: For web_server - base64url is fully URL-safe (uses '-' and '_').
  /// @note Decoding happens at perform() time, directly into the transmit buffer.
  Call &set_raw_timings_base64url(const std::string &base64url) {
    this->base64url_ptr_ = &base64url;
    this->raw_timings_ = nullptr;
    this->packed_data_ = nullptr;
    return this->self_();
  }

  /// Set the raw timings from packed protobuf sint32 data (zigzag + varint encoded)
  /// @note Lifetime: Stores a pointer to the buffer. The buffer must outlive perform().
  /// @note Usage: For API component where data comes directly from the protobuf message.
  Call &set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count) {
    this->packed_data_ = data;
    this->packed_length_ = length;
    this->packed_count_ = count;
    this->raw_timings_ = nullptr;
    this->base64url_ptr_ = nullptr;
    return this->self_();
  }

  /// Set the number of times to repeat transmission (1 = transmit once, 2 = transmit twice, etc.)
  Call &set_repeat_count(uint32_t count) {
    this->repeat_count_ = count;
    return this->self_();
  }

#if defined(USE_API) && defined(USE_IR_RF)
  /// Reply to this API client once the frame has left the transmitter (API 1.18+)
  Call &set_api_connection(api::APIConnection *conn) {
    this->api_connection_ = conn;
    return this->self_();
  }
#endif

  /// Perform the transmission; returns true if a frame was handed to the transmitter
  bool perform() {
    Entity *parent = this->parent_;
    if (parent == nullptr)
      return false;
    parent->on_call_(this->self_());
#if defined(USE_API) && defined(USE_IR_RF)
    // Before control(): blocking transmitters report completion from inside it, and the
    // non-blocking RMT path flushes the previous frame's completion there
    if (this->api_connection_ != nullptr)
      parent->expect_api_reply_(this->api_connection_);
#endif
    const bool started = parent->control(this->self_());
#if defined(USE_API) && defined(USE_IR_RF)
    if (this->api_connection_ != nullptr)
      parent->settle_api_reply_(started);
#endif
    return started;
  }

 protected:
  Call &self_() { return static_cast<Call &>(*this); }

  Entity *parent_;
};

}  // namespace esphome::ir_rf_base
