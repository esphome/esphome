// The proxy's per-slot connection wrapper, shared by every platform.
//
// SERVICE STREAMING HAZARD - read before touching the streaming code here or
// in the platform streamers (bluetooth_connection_bluedroid.cpp).
//
// A V3 client caches the service list it receives as the device's complete,
// permanent database. Nothing on the wire marks a list as partial, so a
// stream that is truncated, has a skipped batch, or is terminated early
// would be cached whole and poison every later session with the device.
//
// The rule: it is always better to send nothing and let the client time out
// than to let services-done follow an incomplete stream. Concretely:
//   - a refused batch rewinds the cursor and is retried, never skipped;
//   - services-done is sent only after every batch was accepted;
//   - every interruption (subscriber lost or swapped, backend abort,
//     bounds-check failure) parks or aborts WITHOUT services-done and drops
//     any owed done;
//   - a new GetServices supersedes an owed done, so a stale done can never
//     land on a fresh request's empty accumulator and cache it as empty.
// The client only caches a list terminated by services-done within the same
// request; timeouts, disconnects and errors raise instead of caching.
#include "bluetooth_connection_hub.h"

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection";

void BluetoothConnection::set_address(uint64_t address) {
  // Keep the proxy's pre-allocated connections-free message in step
  this->proxy_->update_address_slot_(this->address_, address);
  // Slot changing hands: anything owed belonged to the old address. The
  // choke point for every reassignment, not just reset_connection_()'s path.
  this->clear_owed_flags_();
  this->address_ = address;
  if (address == 0) {
    this->address_str_[0] = '\0';
    return;
  }
  uint8_t mac[MAC_ADDRESS_SIZE];
  ble_device_base::uint64_to_mac_msb_first(address, mac);
  format_mac_addr_upper(mac, this->address_str_);
}

void BluetoothConnection::initiate_connection(uint8_t address_type) {
  // No connect timeout here: the API client's own timeout or the api-gone
  // sweep drives disconnect().
  this->state_ = ClientState::CONNECTING;
  int err = this->backend_->connect(this->address_, address_type);
  if (err != 0) {
    ESP_LOGW(TAG, "[%d] [%s] connect failed, err=%d", this->connection_index_, this->address_str_, err);
    this->reset_connection_(err);
  }
}

void BluetoothConnection::disconnect() {
  // Idempotent: the proxy's teardown loop calls this every 100 ms while the
  // API subscriber is gone, and a repeat call reaching the backend would
  // re-arm its teardown timer so the safety timeout never fires.
  if (this->state_ == ClientState::IDLE || this->state_ == ClientState::DISCONNECTING) {
    return;
  }
  int err = this->backend_->gatt_disconnect();
  if (err != 0) {
    // Nonzero means nothing to tear down (both backends): free the slot.
    // Accepted teardowns always reach a terminal report.
    ESP_LOGW(TAG, "[%d] [%s] disconnect while backend idle, err=%d", this->connection_index_, this->address_str_, err);
    this->reset_connection_(err);
    return;
  }
  this->state_ = ClientState::DISCONNECTING;
}

void BluetoothConnection::on_pairing_result(int status) {
  if (this->address_ == 0) {
    // A drop before completion already answered: reset_connection_slot_ sends
    // the connection response, which the client's pair watcher raises on.
    return;
  }
  this->paired_ = status == 0;
  this->proxy_->send_device_pairing(this->address_, status == 0, status);
}

void BluetoothConnection::reset_connection_(conn_err_t reason) {
  if (this->pending_error_ != 0) {
    reason = this->pending_error_;
    this->pending_error_ = 0;
  }
  this->state_ = ClientState::IDLE;
  this->services_discovered_ = false;
  this->paired_ = false;
  // Link gone: the slot may hold a different device before the drain runs.
  this->clear_owed_flags_();
  this->backend_->release_services();
  this->proxy_->reset_connection_slot_(this, reason);
}

// ---- backend event listener ----

void BluetoothConnection::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (connected && this->address_ == 0) {
    // Late completion for a slot that was already freed: nothing to report,
    // and the api-gone sweep or a new reservation owns the slot now.
    // Return ignored: nonzero just means the backend was already idle, and
    // re-arming a freed slot could clobber a new reservation.
    this->backend_->gatt_disconnect();
    return;
  }
  if (connected && this->state_ == ClientState::DISCONNECTING) {
    // The link came up after a disconnect request won the race; finish the
    // teardown instead of reporting a connection the client no longer wants.
    int err = this->backend_->gatt_disconnect();
    if (err != 0) {
      // Nothing left to tear down after all.
      this->reset_connection_(err);
    }
    return;
  }
  if (connected) {
    this->mtu_ = mtu;
    if (this->connection_type_ == ConnectionType::V3_WITH_CACHE) {
      // The API client has the services cached; never discover them. No
      // discovery phase needs the fast interval, so settle straight into the
      // shared steady-state parameters. Both backends already open cached
      // connections with these values (esp32 prefer-params, rp2 initiating
      // params), so this request is normally redundant - kept as a backstop
      // in case the initial parameters were negotiated away.
      this->state_ = ClientState::ESTABLISHED;
      // The one D-level line for a cached connect; the uncached path narrates
      // through "Discovery finished" instead.
      ESP_LOGD(TAG, "[%d] [%s] Connected with cached services, sending connected (mtu=%u)", this->connection_index_,
               this->address_str_, mtu);
      int param_err = this->backend_->update_connection_params(ble_device_base::MEDIUM_MIN_CONN_INTERVAL,
                                                               ble_device_base::MEDIUM_MAX_CONN_INTERVAL, 0,
                                                               ble_device_base::MEDIUM_CONN_TIMEOUT);
      if (param_err != 0) {
        // Survivable: the link just stays on the fast interval.
        ESP_LOGW(TAG, "[%d] [%s] conn param update failed, err=%d", this->connection_index_, this->address_str_,
                 param_err);
      }
      this->send_connected_reply_();
      this->proxy_->send_connections_free();
      return;
    }
    // V3_WITHOUT_CACHE: discover services first — the connected response is
    // sent when discovery completes (MTU + services before the response).
    this->state_ = ClientState::CONNECTED;
    int err = this->backend_->discover_services();
    if (err != 0) {
      ESP_LOGW(TAG, "[%d] [%s] discover_services failed, err=%d", this->connection_index_, this->address_str_, err);
      // Latch the real cause for the disconnect report.
      this->latch_pending_error_(err);
      this->disconnect();
    }
    return;
  }
  // Disconnected, connect failed, or teardown complete
  if (this->address_ == 0) {
    return;  // Slot already freed
  }
  ESP_LOGD(TAG, "[%d] [%s] Disconnected, reason=0x%02x, freeing slot", this->connection_index_, this->address_str_,
           error);
  this->reset_connection_(error);
}

void BluetoothConnection::on_service_discovery_done(int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "[%d] [%s] Service discovery failed, err=%d", this->connection_index_, this->address_str_, error);
    // Carry the GATT error into the disconnection report so the client sees
    // the real cause instead of a generic HCI reason.
    this->latch_pending_error_(error);
    this->disconnect();
    return;
  }
  ESP_LOGD(TAG, "[%d] [%s] Discovery finished, sending connected (mtu=%u)", this->connection_index_, this->address_str_,
           this->mtu_);
  this->state_ = ClientState::ESTABLISHED;
  this->services_discovered_ = true;
  this->send_connected_reply_();
  this->proxy_->send_connections_free();
}

void BluetoothConnection::flush_owed_replies_() {
  // Connected first: the client should never see services-done or an ack for
  // a link it has not been told is up. Structural, not size-dependent: a
  // still-owed connected reply defers the smaller sends to the next tick.
  if (this->connected_reply_owed_) {
    this->send_connected_reply_();
    if (this->connected_reply_owed_) {
      // The retry limits are wall-clock windows: age the deferred budgets so
      // a reply cannot outlive the window it was sized for.
      if (this->send_service_ == SERVICES_DONE_PENDING) {
        this->age_services_done_();
      }
      if (this->has_pending_ack_()) {
        this->age_pending_ack_();
      }
      return;
    }
  }
  if (this->send_service_ == SERVICES_DONE_PENDING) {
    this->send_services_done_();
  }
  if (this->has_pending_ack_()) {
    this->flush_pending_ack_();
  }
}

void BluetoothConnection::send_connected_reply_() {
  if (this->proxy_->send_device_connection(this->address_, true, this->mtu_)) {
    this->connected_reply_owed_ = false;
    return;
  }
  // Warn on the leading edge only, as elsewhere: the drop must be visible but
  // must not add traffic to the connection that just refused a frame.
  if (!this->connected_reply_owed_) {
    ESP_LOGW(TAG, "[%d] [%s] Connected reply deferred, TCP buffer full", this->connection_index_, this->address_str_);
    this->connected_reply_owed_ = true;
  }
}

void BluetoothConnection::log_gatt_operation_error_(const char *operation, uint16_t handle, int status) {
  ESP_LOGW(TAG, "[%d] [%s] Error %s for handle 0x%2X, status=%d", this->connection_index_, this->address_str_,
           operation, handle, status);
}

void BluetoothConnection::note_batch_stalled_() {
  if (this->batch_stalled_)
    return;
  this->batch_stalled_ = true;
  ESP_LOGW(TAG, "[%d] [%s] Service batch deferred, TCP buffer full; retrying", this->connection_index_,
           this->address_str_);
}

/// Both payload-free acks are just (address, handle); only the type differs.
template<typename Response>
static bool send_handle_reply(api::APIConnection *api_connection, uint64_t address, uint16_t handle) {
  Response resp;
  resp.address = address;
  resp.handle = handle;
  return api_connection->send_message(resp);
}

/// Sole construction site, so a re-offer cannot drift from the original.
bool BluetoothConnection::try_send_ack_(PendingAck kind, uint16_t handle, conn_err_t error) {
  if (kind == PendingAck::PENDING_ACK_ERROR) {
    // Proxy owns the error reply and reports a refusal the same way.
    return this->proxy_->send_gatt_error(this->address_, handle, error);
  }
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return true;  // Nobody subscribed: nothing is owed
  switch (kind) {
    case PendingAck::PENDING_ACK_WRITE:
      return send_handle_reply<api::BluetoothGATTWriteResponse>(api_connection, this->address_, handle);
    case PendingAck::PENDING_ACK_NOTIFY:
      return send_handle_reply<api::BluetoothGATTNotifyResponse>(api_connection, this->address_, handle);
    case PendingAck::PENDING_ACK_NONE:
    case PendingAck::PENDING_ACK_ERROR:  // returned above
      return true;
  }
  // No default label above, so a new enumerator is a -Wswitch warning rather
  // than a silent notify reply. This return only satisfies -Wreturn-type.
  return true;
}

void BluetoothConnection::send_ack_(PendingAck kind, uint16_t handle, conn_err_t error) {
  if (this->try_send_ack_(kind, handle, error))
    return;
  // Report a newly owed reply and a displaced one; displacing is the case
  // that loses a reply. Re-refusing the same one stays quiet.
  if (!this->has_pending_ack_()) {
    ESP_LOGW(TAG, "[%d] [%s] GATT reply for handle 0x%04X deferred, TCP buffer full", this->connection_index_,
             this->address_str_, handle);
  } else if (this->pending_ack_handle_ != handle || this->pending_ack_ != kind) {
    ESP_LOGW(TAG, "[%d] [%s] GATT reply for handle 0x%04X dropped for handle 0x%04X", this->connection_index_,
             this->address_str_, this->pending_ack_handle_, handle);
  }
  this->latch_pending_ack_(kind, handle, error);
}

void BluetoothConnection::flush_pending_ack_() {
  if (!this->has_pending_ack_())
    return;
  if (this->try_send_ack_(this->pending_ack_, this->pending_ack_handle_, this->pending_ack_error_)) {
    this->clear_pending_ack_();
    return;
  }
  this->age_pending_ack_();
}

void BluetoothConnection::age_pending_ack_() {
  if (++this->pending_ack_retries_ >= PENDING_ACK_RETRY_LIMIT) {
    // Undeliverable: past here the client has given up and may have re-asked,
    // and a late reply would answer the new request instead of this one.
    ESP_LOGW(TAG, "[%d] [%s] GATT reply for handle 0x%04X undeliverable, abandoning", this->connection_index_,
             this->address_str_, this->pending_ack_handle_);
    this->clear_pending_ack_();
  }
}

void BluetoothConnection::on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {
  // Late completion for a freed slot; nothing to report.
  if (this->address_ == 0)
    return;
  if (error != 0) {
    this->log_gatt_operation_error_("reading char/descriptor", handle, error);
    this->send_gatt_error_(handle, error);
    return;
  }
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return;
  api::BluetoothGATTReadResponse resp;
  resp.address = this->address_;
  resp.handle = handle;
  resp.set_data(data, len);
  if (!api_connection->send_message(resp)) {
    // Not latched: would mean holding the payload through the congestion
    // that refused it. The client's read timeout arbitrates.
    ESP_LOGW(TAG, "[%d] [%s] Failed to send read response", this->connection_index_, this->address_str_);
  }
}

void BluetoothConnection::on_write_result(uint16_t handle, int error) {
  if (this->address_ == 0)
    return;
  if (error != 0) {
    this->log_gatt_operation_error_("writing char/descriptor", handle, error);
    this->send_gatt_error_(handle, error);
    return;
  }
  this->send_ack_(PendingAck::PENDING_ACK_WRITE, handle);
}

void BluetoothConnection::on_notify_state(uint16_t handle, bool enabled, int error) {
  if (this->address_ == 0)
    return;
  if (error != 0) {
    this->log_gatt_operation_error_(enabled ? "registering notifications" : "unregistering notifications", handle,
                                    error);
    this->send_gatt_error_(handle, error);
    return;
  }
  this->send_ack_(PendingAck::PENDING_ACK_NOTIFY, handle);
}

void BluetoothConnection::on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (this->address_ == 0)
    return;
  ESP_LOGV(TAG, "[%d] [%s] Notify: handle=0x%2X", this->connection_index_, this->address_str_, handle);
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return;
  api::BluetoothGATTNotifyDataResponse resp;
  resp.address = this->address_;
  resp.handle = handle;
  resp.set_data(data, len);
  if (!api_connection->send_message(resp)) {
    // Not latched, same reason as the read reply. Notify data is lossy: the
    // peripheral will not resend it.
    ESP_LOGW(TAG, "[%d] [%s] Failed to send notify data response", this->connection_index_, this->address_str_);
  }
}

// ---- GATT operations ----

conn_err_t BluetoothConnection::check_connected_op_(const char *action, const char *type) const {
  if (this->connected()) {
    return CONN_OK;
  }
  ESP_LOGW(TAG, "[%d] [%s] Cannot %s GATT %s, not connected.", this->connection_index_, this->address_str_, action,
           type);
  return GATT_NOT_CONNECTED;
}

conn_err_t BluetoothConnection::read_characteristic(uint16_t handle) {
  this->supersede_pending_ack_(handle, PendingAck::PENDING_ACK_NONE);
  if (conn_err_t err = this->check_connected_op_("read", "characteristic"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Reading GATT characteristic handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->read_characteristic(handle);
}

conn_err_t BluetoothConnection::write_characteristic(uint16_t handle, const uint8_t *data, size_t length,
                                                     bool response) {
  this->supersede_pending_ack_(handle, PendingAck::PENDING_ACK_WRITE);
  if (conn_err_t err = this->check_connected_op_("write", "characteristic"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT characteristic handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->write_characteristic(handle, data, static_cast<uint16_t>(length), response);
}

conn_err_t BluetoothConnection::read_descriptor(uint16_t handle) {
  this->supersede_pending_ack_(handle, PendingAck::PENDING_ACK_NONE);
  if (conn_err_t err = this->check_connected_op_("read", "descriptor"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Reading GATT descriptor handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->read_descriptor(handle);
}

// The neutral backend contract performs descriptor writes acknowledged, so
// the response flag is intentionally ignored (esp32 maps it to RSP/NO_RSP).
conn_err_t BluetoothConnection::write_descriptor(uint16_t handle, const uint8_t *data, size_t length,
                                                 bool /*response*/) {
  this->supersede_pending_ack_(handle, PendingAck::PENDING_ACK_WRITE);
  if (conn_err_t err = this->check_connected_op_("write", "descriptor"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT descriptor handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->write_descriptor(handle, data, static_cast<uint16_t>(length));
}

conn_err_t BluetoothConnection::notify_characteristic(uint16_t handle, bool enable) {
  this->supersede_pending_ack_(handle, PendingAck::PENDING_ACK_NOTIFY);
  if (conn_err_t err = this->check_connected_op_("notify", "characteristic"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] %s GATT characteristic notifications handle %d", this->connection_index_, this->address_str_,
           enable ? "Registering for" : "Unregistering for", handle);
  return this->backend_->notify_characteristic(handle, enable);
}

conn_err_t BluetoothConnection::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                                         uint16_t timeout) {
  if (conn_err_t err = this->check_connected_op_("update params of", "connection"); err != CONN_OK)
    return err;
  return this->backend_->update_connection_params(min_interval, max_interval, latency, timeout);
}

// ---- Service streaming ----

void BluetoothConnection::send_services_done_() {
  if (this->proxy_->send_gatt_services_done(this->address_)) {
    // Sent, or subscriber gone (park silently; its timeout arbitrates).
    this->send_service_ = DONE_SENDING_SERVICES;
    return;
  }
  if (this->send_service_ != SERVICES_DONE_PENDING) {
    // Warn on the transition only; retries stay silent.
    ESP_LOGW(TAG, "[%d] [%s] Failed to send services done, retrying", this->connection_index_, this->address_str_);
    this->services_done_retries_ = 0;
    this->send_service_ = SERVICES_DONE_PENDING;
  } else {
    this->age_services_done_();
  }
}

void BluetoothConnection::age_services_done_() {
  if (++this->services_done_retries_ >= SERVICES_DONE_RETRY_LIMIT) {
    // Undeliverable (see SERVICES_DONE_RETRY_LIMIT); silence arbitrates.
    ESP_LOGW(TAG, "[%d] [%s] Services done undeliverable, abandoning", this->connection_index_, this->address_str_);
    this->send_service_ = DONE_SENDING_SERVICES;
  }
}

void BluetoothConnection::send_service_for_discovery_() {
  auto table = this->backend_->get_service_table();
  if (this->send_service_ >= table.service_count) {
    this->backend_->release_services();
    this->send_services_done_();
    return;
  }

  // The subscriber vanished mid-stream; the api-gone sweep tears the
  // connection down anyway.
  auto *api_conn = this->proxy_->get_api_connection();
  if (api_conn == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] API connection lost while streaming services", this->connection_index_,
             this->address_str_);
    this->park_service_stream_();
    return;
  }

  // Check if client supports efficient UUIDs
  bool use_efficient_uuids = this->proxy_->client_supports_efficient_uuids();

  // Prepare response
  api::BluetoothGATTGetServicesResponse resp;
  resp.address = this->address_;

  // Dynamic batching based on actual size, same contract as the esp32 streamer
  size_t current_size = resp.calculate_size();
  int16_t batch_start = this->send_service_;

  while (this->send_service_ < table.service_count) {
    const auto &service = table.services[this->send_service_];

    // If this service likely won't fit, send current batch (unless it's the first)
    size_t estimated_size = estimate_service_size(service.characteristic_count, use_efficient_uuids);
    if (!resp.services.empty() && (current_size + estimated_size > MAX_PACKET_SIZE)) {
      break;
    }

    resp.services.emplace_back();
    auto &service_resp = resp.services.back();
    fill_gatt_uuid(service_resp.uuid, service_resp.short_uuid, service.uuid, use_efficient_uuids);
    service_resp.handle = service.start_handle;

    // Bounds-check the backend's index ranges against the table totals rather
    // than trusting its discovery bookkeeping blindly. A miscounted non-empty
    // range must not stream a truncated database as authoritative (V3 clients
    // cache it permanently): abort and tear the connection down; the client
    // times out and retries. Empty ranges are tolerated regardless of index.
    uint16_t char_count = service.characteristic_count;
    if (char_count != 0 && service.first_characteristic + char_count > table.characteristic_count) {
      ESP_LOGE(TAG, "[%d] [%s] Characteristic range out of bounds (service %d), aborting stream",
               this->connection_index_, this->address_str_, this->send_service_);
      this->abort_service_stream(ble_device_base::GATT_ERR_UNLIKELY);
      return;
    }
    if (char_count > 0) {
      service_resp.characteristics.init(char_count);
      for (uint16_t ci = 0; ci < char_count; ci++) {
        const auto &chr = table.characteristics[service.first_characteristic + ci];
        service_resp.characteristics.emplace_back();
        auto &characteristic_resp = service_resp.characteristics.back();
        fill_gatt_uuid(characteristic_resp.uuid, characteristic_resp.short_uuid, chr.uuid, use_efficient_uuids);
        characteristic_resp.handle = chr.value_handle;
        characteristic_resp.properties = chr.properties;
        uint16_t desc_count = chr.descriptor_count;
        if (desc_count != 0 && chr.first_descriptor + desc_count > table.descriptor_count) {
          ESP_LOGE(TAG, "[%d] [%s] Descriptor range out of bounds (service %d), aborting stream",
                   this->connection_index_, this->address_str_, this->send_service_);
          this->abort_service_stream(ble_device_base::GATT_ERR_UNLIKELY);
          return;
        }
        if (desc_count == 0) {
          continue;
        }
        characteristic_resp.descriptors.init(desc_count);
        for (uint16_t di = 0; di < desc_count; di++) {
          const auto &desc = table.descriptors[chr.first_descriptor + di];
          characteristic_resp.descriptors.emplace_back();
          auto &descriptor_resp = characteristic_resp.descriptors.back();
          fill_gatt_uuid(descriptor_resp.uuid, descriptor_resp.short_uuid, desc.uuid, use_efficient_uuids);
          descriptor_resp.handle = desc.handle;
        }
      }
    }

    if (close_service_batch(resp, current_size, this->send_service_, this->connection_index_, this->address_str_) !=
        BatchClose::CONTINUE) {
      break;
    }
  }

  // Send the message with dynamically batched services; on a failed send,
  // rewind the cursor so the batch is retried instead of silently skipped
  // (bounded: a subscriber that stays gone ends streaming via the api-lost
  // rewind above).
  if (!api_conn->send_message(resp)) {
    this->note_batch_stalled_();
    this->send_service_ = batch_start;
    return;
  }
  this->batch_stalled_ = false;
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS
