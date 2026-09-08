// BluetoothConnection: drives the build's GATT backend (the
// ble_device_base::BLEGattConnection alias) and translates its events into
// the proxy's API messages. One wrapper for every platform; per-backend
// differences live behind the alias and the streamer cut-through.

#pragma once

#include "bluetooth_connection.h"

// The wrapper exists to serve the proxy's API surface; direct consumers
// drive the backend themselves, so backend-only builds compile this header
// empty.
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "bluetooth_connection_gatt_backend.h"
#include "esphome/core/helpers.h"

namespace esphome::bluetooth_proxy {
class BluetoothProxy;
}  // namespace esphome::bluetooth_proxy

namespace esphome::bluetooth_connection {

using ClientState = ble_device_base::ClientState;
using ConnectionType = ble_device_base::ConnectionType;

/// A refused GATT reply owed to the current subscriber. Payload-free only:
/// these rebuild from address + handle + error, so a retry costs no buffered
/// data. Read and notify-data carry payloads and are deliberately absent.
enum class PendingAck : uint8_t {
  PENDING_ACK_NONE = 0,
  PENDING_ACK_WRITE,
  PENDING_ACK_NOTIFY,
  PENDING_ACK_ERROR,
};

class BluetoothConnection final : public ble_device_base::GattClientListener {
 public:
  /// Wire the platform backend. Called from codegen before setup.
  void set_backend(ble_device_base::BLEGattConnection *backend) {
    this->backend_ = backend;
    backend->set_listener(this);
  }

  // ---- proxy dispatch surface ----
  conn_err_t read_characteristic(uint16_t handle);
  conn_err_t write_characteristic(uint16_t handle, const uint8_t *data, size_t length, bool response);
  conn_err_t read_descriptor(uint16_t handle);
  conn_err_t write_descriptor(uint16_t handle, const uint8_t *data, size_t length, bool response);
  conn_err_t notify_characteristic(uint16_t handle, bool enable);
  conn_err_t update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);

  /// Streamer abort: latch the GATT cause, park the cursor, tear down.
  void abort_service_stream(conn_err_t err) {
    this->latch_pending_error_(err);
    this->send_service_ = DONE_SENDING_SERVICES;
    this->disconnect();
  }

  /// Start connecting with the API address type (BLE_ADDR_TYPE_* code
  /// space). Failures report through the same reset path a failed open
  /// takes.
  void initiate_connection(uint8_t address_type);
  void disconnect();
  /// A connect request racing a scheduled teardown: true when the backend
  /// had not started closing - the in-flight open resumes and reports
  /// connected. False once the teardown owns the link.
  bool cancel_teardown() {
    if (this->state_ == ClientState::DISCONNECTING && this->backend_->cancel_gatt_disconnect()) {
      this->state_ = ClientState::CONNECTING;
      return true;
    }
    return false;
  }
  bool is_paired() const { return this->paired_; }
  void set_unpaired() { this->paired_ = false; }
  conn_err_t pair() { return this->backend_->pair(); }

  void set_address(uint64_t address);
  uint64_t get_address() const { return this->address_; }
  const char *address_str() const { return this->address_str_; }
  uint8_t get_connection_index() const { return this->connection_index_; }

  ClientState state() const { return this->state_; }
  void set_state(ClientState st) { this->state_ = st; }
  bool connected() const { return this->state_ == ClientState::ESTABLISHED; }
  void set_connection_type(ConnectionType ct) {
    this->connection_type_ = ct;
    // Both backends branch on the type before connecting (bluedroid picks
    // prefer-params and the with-cache report at OPEN_EVT; rp2 picks the
    // initiating parameters), so this must be set before the connect starts.
    this->backend_->set_connection_type(ct);
  }
  // Latched at discovery completion rather than read from the backend table:
  // streaming frees the table, and this must stay true for the connection's
  // lifetime (a repeat GetServices is silently ignored, never answered with
  // an authoritative empty database).
  bool has_gatt_services() const { return this->services_discovered_; }

  /// Stream any pending service-discovery batch (proxy loop; the backend
  /// owns the disconnect safety timer).
  void process_pending_services() {
    if (this->send_service_ >= 0) {
      this->stream_pending_(this->backend_);
    }
  }

  // ---- backend event listener (called directly by the backend, main loop) ----
  void on_connection_state(bool connected, uint16_t mtu, int error) override;
  void on_service_discovery_done(int error) override;
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) override;
  void on_write_result(uint16_t handle, int error) override;
  void on_notify_state(uint16_t handle, bool enabled, int error) override;
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) override;
  void on_pairing_result(int status) override;

 protected:
  friend class bluetooth_proxy::BluetoothProxy;
  // The Bluedroid backend streams services in place from its stack cache.
  friend class BluedroidGattClient;

  /// First cause wins: a later, less specific error must not overwrite it.
  void latch_pending_error_(conn_err_t err) {
    if (this->pending_error_ == 0) {
      this->pending_error_ = err;
    }
  }

  /// Latch a refused reply for the proxy drain. One slot per connection,
  /// newest wins: a GATT client works one request at a time, and a discarded
  /// reply falls back to the timeout it would have hit anyway.
  void latch_pending_ack_(PendingAck kind, uint16_t handle, conn_err_t error = 0) {
    this->pending_ack_retries_ = 0;
    this->pending_ack_ = kind;
    this->pending_ack_handle_ = handle;
    this->pending_ack_error_ = error;
  }
  void clear_pending_ack_() { this->pending_ack_ = PendingAck::PENDING_ACK_NONE; }
  /// Drop an owed reply this re-ask makes stale. Clients match futures on
  /// response type as well as handle, so an owed error (which resolves any op
  /// on the handle) is cleared by any re-ask, other kinds only by their own.
  void supersede_pending_ack_(uint16_t handle, PendingAck kind) {
    if (this->has_pending_ack_() && this->pending_ack_handle_ == handle &&
        (this->pending_ack_ == PendingAck::PENDING_ACK_ERROR || this->pending_ack_ == kind)) {
      this->clear_pending_ack_();
    }
  }
  bool has_pending_ack_() const { return this->pending_ack_ != PendingAck::PENDING_ACK_NONE; }
  /// Warn on the stall's leading edge only. The batch is never lost (the
  /// caller rewinds the cursor), and a warning per attempt would add traffic
  /// to the connection already refusing frames. Both streamers route here.
  void note_batch_stalled_();
  /// Send the connected=true reply, latching it if the API refuses. Rebuilt
  /// from address_ and mtu_, so the latch is one bit; a dropped confirmation
  /// leaves the client timing out while this slot holds a live link. No retry
  /// bound: the slot's lifetime is the bound (teardown clears the flag).
  void send_connected_reply_();
  /// Re-offer everything this slot owes. One entry point so the proxy drain
  /// does not have to know which latches exist.
  void flush_owed_replies_();
  /// Drop everything this slot owes, in one write to the shared tail byte.
  void clear_owed_flags_() {
    this->pending_ack_ = PendingAck::PENDING_ACK_NONE;
    this->batch_stalled_ = false;
    this->connected_reply_owed_ = false;
    this->ack_deferred_warned_ = false;
    this->notify_drop_warned_ = false;
  }
  /// Sole construction site for these replies, shared by send and retry.
  bool try_send_ack_(PendingAck kind, uint16_t handle, conn_err_t error);
  /// First attempt: send, and latch it for the drain if the API refuses.
  void send_ack_(PendingAck kind, uint16_t handle, conn_err_t error = 0);
  /// Report a rejected request. Latched like a completion reply, so a
  /// refused frame does not strand the client for its whole timeout.
  void send_gatt_error_(uint16_t handle, conn_err_t error) {
    this->send_ack_(PendingAck::PENDING_ACK_ERROR, handle, error);
  }
  /// Re-offer the owed reply; clears on success, stays owed on a refusal.
  void flush_pending_ack_();
  /// Advance the retry budget and abandon at the limit, without sending.
  void age_pending_ack_();
  // A backend providing its own streamer (see the contract doc) builds the
  // response in place from its stack cache; the rest use the table streamer.
  // Template so the discarded branch is not odr-checked against backends
  // that lack the method.
  template<typename Backend> void stream_pending_(Backend *backend) {
    if constexpr (requires { backend->stream_service_batch(*this); }) {
      backend->stream_service_batch(*this);
    } else {
      this->send_service_for_discovery_();
    }
  }
  /// Park the stream without services-done and free any held table: an
  /// interrupted stream must never be declared complete (the client's
  /// timeout arbitrates), and an owed done is dropped with it.
  void park_service_stream_() {
    this->batch_stalled_ = false;
    if (this->send_service_ >= 0) {
      this->backend_->release_services();
      this->send_service_ = DONE_SENDING_SERVICES;
    } else if (this->send_service_ == SERVICES_DONE_PENDING) {
      this->send_service_ = DONE_SENDING_SERVICES;
    }
  }
  void send_service_for_discovery_();
  /// Send services-done and settle the cursor: DONE when it lands (or no
  /// subscriber), SERVICES_DONE_PENDING on a refused frame (proxy drain
  /// retries). Callers release the table first; the message needs only the
  /// address.
  void send_services_done_();
  /// Advance the retry budget and abandon at the limit, without sending.
  void age_services_done_();
  void reset_connection_(conn_err_t reason);
  conn_err_t check_connected_op_(const char *action, const char *type) const;
  void log_gatt_operation_error_(const char *operation, uint16_t handle, int status);

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  bluetooth_proxy::BluetoothProxy *proxy_{nullptr};
  ble_device_base::BLEGattConnection *backend_{nullptr};

  // Group 2: 2-byte types. Exactly 4 bytes, so address_ below stays
  // 8-aligned with no padding (the vptr makes Group 1 12 bytes, not 8).
  int16_t send_service_{INIT_SENDING_SERVICES};
  uint16_t mtu_{ble_device_base::DEFAULT_ATT_MTU};

  // Group 3: 8-byte and 4-byte types
  uint64_t address_{0};
  conn_err_t pending_error_{0};
  // Full width: the GATT error domain is open-ended (ble_gatt_client.h) and
  // forwarded untranslated, so narrowing would corrupt platform codes.
  conn_err_t pending_ack_error_{0};

  // Group 4: Arrays
  char address_str_[MAC_ADDRESS_PRETTY_BUFFER_SIZE]{};
  // Parked here rather than in Group 2: address_str_ ends 2-aligned, so this
  // uses tail slack instead of pushing address_ out by 6 bytes of padding.
  uint16_t pending_ack_handle_{0};

  // Group 5: bit-packed tail. The first two bytes were already full, so the
  // first added bit forced a third and took the 8-aligned object 48 -> 56;
  // the handle, error and retry counter ride in that padding. Two bitfield
  // bits left; another byte-sized member costs 8 per slot.
  static_assert(static_cast<uint8_t>(ClientState::ESTABLISHED) < (1 << 3), "state_ bitfield too narrow");
  static_assert(static_cast<uint8_t>(ConnectionType::V3_WITHOUT_CACHE) < (1 << 2),
                "connection_type_ bitfield too narrow");
  // Ordered so neither byte's fields straddle a storage unit: 3+5 and
  // 4+2+1+1 fill the first two tail bytes exactly.
  ClientState state_ : 3 {ClientState::IDLE};
  static_assert(SERVICES_DONE_RETRY_LIMIT < (1 << 5), "counter bitfield too narrow");
  uint8_t services_done_retries_ : 5 {0};
  uint8_t connection_index_ : 4 {0};
  ConnectionType connection_type_ : 2 {ConnectionType::V1};
  bool paired_ : 1 {false};
  bool services_discovered_ : 1 {false};
  static_assert(static_cast<uint8_t>(PendingAck::PENDING_ACK_ERROR) < (1 << 2), "pending_ack_ bitfield too narrow");
  PendingAck pending_ack_ : 2 {PendingAck::PENDING_ACK_NONE};
  /// Set while a refused batch is retrying, so only the first one warns.
  bool batch_stalled_ : 1 {false};
  /// An owed connected=true reply; the proxy's paced drain re-offers it.
  bool connected_reply_owed_ : 1 {false};
  /// Set once the deferred warn fired; with an unchanged pending_ack_handle_
  /// it keeps re-deferrals of the same handle quiet (see send_ack_).
  bool ack_deferred_warned_ : 1 {false};
  /// Set on the first dropped notify; later drops log at verbose only.
  bool notify_drop_warned_ : 1 {false};
  // Plain byte after the bitfields: takes the padding byte instead of
  // straddling pending_ack_'s storage unit and growing the object.
  static_assert(PENDING_ACK_RETRY_LIMIT <= 0xFF, "retry counter too narrow");
  uint8_t pending_ack_retries_{0};
};

// Pins the grouping above: pending_ack_handle_ in Group 2 instead would pad
// address_ out and reach 64. 32-bit only; the host unit tests build 64-bit.
static_assert(sizeof(void *) != 4 || sizeof(BluetoothConnection) <= 56,
              "BluetoothConnection layout regressed on a 32-bit target");

}  // namespace esphome::bluetooth_connection

#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS
