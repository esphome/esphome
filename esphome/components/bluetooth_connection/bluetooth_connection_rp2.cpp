#include "bluetooth_connection_rp2.h"

#include "bluetooth_connection_hub.h"
#include "bluetooth_connection.h"

#if defined(USE_RP2040_BLE) && defined(USE_BLE_GATT_CLIENT)

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <BluetoothLock.h>

#include <cstring>
#include <new>

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection.rp2";

using ble_device_base::ESPBTUUID;
using ble_device_base::GATT_ERR_NOT_CONNECTED;
using ble_device_base::GATT_ERR_NO_MEMORY;

// Engine-owned timeouts: BTstack has a 30 s ATT transaction timeout but no
// connect timeout — a stuck LE_CONNECTING both blocks future gap_connect calls
// and keeps the scan inhibited, so the engine cancels after 20 s. The
// disconnect timeout mirrors the esp32 CLOSE_EVT safety net.
static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
static constexpr uint32_t DISCONNECT_TIMEOUT_MS = 10000;
// Can-send windows normally open within a connection interval (tens of ms).
static constexpr uint32_t WRITE_NO_RSP_TIMEOUT_MS = 500;

// HCI "connection timeout" reason, reported when a teardown had to be forced.
static constexpr uint8_t HCI_REASON_CONNECTION_TIMEOUT = 0x08;

// Initiating-scan parameters and connection-event lengths for outgoing
// connections (BTstack-specific knobs; the connection intervals themselves are
// the shared FAST/MEDIUM parameters from ble_device_base/ble_client_state.h,
// used in the same lifecycle places as esp32: FAST for connect and service
// discovery, MEDIUM once established).
static constexpr uint16_t CONN_SCAN_INTERVAL = 96;  // 60 ms in 0.625 ms units
static constexpr uint16_t CONN_SCAN_WINDOW = 48;    // 30 ms in 0.625 ms units
static constexpr uint16_t CONN_CE_MIN = 16;         // 10 ms in 0.625 ms units
static constexpr uint16_t CONN_CE_MAX = 48;         // 30 ms in 0.625 ms units

using ble_device_base::FAST_CONN_TIMEOUT;
using ble_device_base::FAST_MAX_CONN_INTERVAL;
using ble_device_base::FAST_MIN_CONN_INTERVAL;
using ble_device_base::MEDIUM_CONN_TIMEOUT;
using ble_device_base::MEDIUM_MAX_CONN_INTERVAL;
using ble_device_base::MEDIUM_MIN_CONN_INTERVAL;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
RP2GattClient *RP2GattClient::instances[ESPHOME_BLE_GATT_CLIENT_COUNT] = {};
uint8_t RP2GattClient::instance_count = 0;
btstack_packet_callback_registration_t RP2GattClient::hci_event_registration = {};
btstack_packet_callback_registration_t RP2GattClient::sm_event_registration = {};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static ESPBTUUID uuid_from_btstack(uint16_t uuid16, const uint8_t uuid128[16]) {
  if (uuid16 != 0) {
    return ESPBTUUID::from_uint16(uuid16);
  }
  // BTstack structs carry the 128-bit form big-endian (printable order).
  return ESPBTUUID::from_raw_reversed(uuid128);
}

void RP2GattClient::setup() {
  // Pre-create every pool entry so the packet handlers' allocate() calls are
  // always a free-list pop -- the IRQ path must never reach malloc().
  if (!this->event_pool_.warm() || !this->notify_pool_.warm()) {
    ESP_LOGE(TAG, "GATT event pool warm-up failed");
    this->mark_failed();
    return;
  }

  // Register this engine for IRQ-context event routing.
  if (instance_count >= ESPHOME_BLE_GATT_CLIENT_COUNT) {
    // Cannot happen with codegen-sized storage; refuse loudly if it ever does.
    ESP_LOGE(TAG, "GATT client registry full");
    this->mark_failed();
    return;
  }
  {
    // One locked section: the slot store lands before the count bump, and a
    // live HCI handler (N > 1 builds) cannot read a half-written registry.
    BluetoothLock lock;
    instances[instance_count] = this;
    instance_count++;
    // One HCI event handler for all engine instances (BTstack supports
    // multiple registrations, so rp2040_ble's own handler is unaffected).
    if (hci_event_registration.callback == nullptr) {
      hci_event_registration.callback = &RP2GattClient::hci_packet_handler;
      hci_add_event_handler(&hci_event_registration);
      sm_event_registration.callback = &RP2GattClient::sm_packet_handler;
      sm_add_event_handler(&sm_event_registration);
    }
  }

  this->disable_loop();
}

float RP2GattClient::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void RP2GattClient::dump_config() { ESP_LOGCONFIG(TAG, "RP2 GATT client (BTstack)"); }

// ---- IRQ-context handlers: copy-and-enqueue only ----

RP2GattClient *RP2GattClient::instance_for_con_handle(hci_con_handle_t con_handle) {
  for (uint8_t i = 0; i < instance_count; i++) {
    if (instances[i]->con_handle_ == con_handle) {
      return instances[i];
    }
  }
  return nullptr;
}

void RP2GattClient::hci_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
  if (type != HCI_EVENT_PACKET) {
    return;
  }
  uint8_t event_type = hci_event_packet_get_type(packet);
  switch (event_type) {
    case HCI_EVENT_META_GAP: {
      if (hci_event_gap_meta_get_subevent_code(packet) != GAP_SUBEVENT_LE_CONNECTION_COMPLETE) {
        break;
      }
      bd_addr_t peer;
      gap_subevent_le_connection_complete_get_peer_address(packet, peer);
      uint8_t status = gap_subevent_le_connection_complete_get_status(packet);
      hci_con_handle_t con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
      // Route to the engine that is waiting for this peer.
      for (uint8_t i = 0; i < instance_count; i++) {
        RP2GattClient *inst = instances[i];
        if (inst->state_ == EngineState::CONNECTING && memcmp(inst->peer_addr_, peer, sizeof(bd_addr_t)) == 0) {
          inst->enqueue_event_irq_(RP2GattEvent::CONNECTED, status, con_handle);
          break;
        }
      }
      break;
    }
    case HCI_EVENT_DISCONNECTION_COMPLETE: {
      hci_con_handle_t con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
      RP2GattClient *inst = instance_for_con_handle(con_handle);
      if (inst == nullptr && instance_count == 1) {
        // The main loop may not have recorded the handle yet (the CONNECTED
        // event is still queued); with a single engine the connecting
        // instance is unambiguous, so route there to close the
        // accept-then-drop window. With multiple engines the event has no
        // address to match on, so it must be dropped instead of guessed.
        RP2GattClient *candidate = instances[0];
        if (candidate->con_handle_ == HCI_CON_HANDLE_INVALID && candidate->state_ != EngineState::IDLE) {
          inst = candidate;
        }
      }
      if (inst != nullptr) {
        inst->enqueue_event_irq_(RP2GattEvent::DISCONNECTED, hci_event_disconnection_complete_get_reason(packet), 0);
      }
      break;
    }
    default:
      break;
  }
}

void RP2GattClient::sm_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
  if (type != HCI_EVENT_PACKET) {
    return;
  }
  switch (hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST:
      // Confirming from the SM callback is the intended BTstack pattern.
      // Unscoped on purpose: no peripheral role exists in-tree, and scoping
      // would drop a request racing the queued CONNECTED event.
      sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
      break;
    case SM_EVENT_PAIRING_COMPLETE: {
      RP2GattClient *inst = instance_for_con_handle(sm_event_pairing_complete_get_handle(packet));
      if (inst != nullptr) {
        inst->enqueue_event_irq_(RP2GattEvent::PAIRING_RESULT, sm_event_pairing_complete_get_status(packet), 0);
      }
      break;
    }
    case SM_EVENT_REENCRYPTION_COMPLETE: {
      // A bonded peer re-encrypts instead of pairing; BTstack emits only this
      // event on that path, so it answers the PAIR request too.
      RP2GattClient *inst = instance_for_con_handle(sm_event_reencryption_complete_get_handle(packet));
      if (inst != nullptr) {
        inst->enqueue_event_irq_(RP2GattEvent::PAIRING_RESULT, sm_event_reencryption_complete_get_status(packet), 0);
      }
      break;
    }
    default:
      break;
  }
}

void RP2GattClient::gatt_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
  if (type != HCI_EVENT_PACKET) {
    return;
  }
  uint8_t event_type = hci_event_packet_get_type(packet);
  // Every GATT event carries the connection handle in the same position via
  // its accessor; route on it.
  hci_con_handle_t con_handle;
  switch (event_type) {
    case GATT_EVENT_MTU:
      con_handle = gatt_event_mtu_get_handle(packet);
      break;
    case GATT_EVENT_SERVICE_QUERY_RESULT:
      con_handle = gatt_event_service_query_result_get_handle(packet);
      break;
    case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
      con_handle = gatt_event_characteristic_query_result_get_handle(packet);
      break;
    case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
      con_handle = gatt_event_all_characteristic_descriptors_query_result_get_handle(packet);
      break;
    case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
      con_handle = gatt_event_long_characteristic_value_query_result_get_handle(packet);
      break;
    case GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
      con_handle = gatt_event_long_characteristic_descriptor_query_result_get_handle(packet);
      break;
    case GATT_EVENT_NOTIFICATION:
      con_handle = gatt_event_notification_get_handle(packet);
      break;
    case GATT_EVENT_INDICATION:
      con_handle = gatt_event_indication_get_handle(packet);
      break;
    case GATT_EVENT_QUERY_COMPLETE:
      con_handle = gatt_event_query_complete_get_handle(packet);
      break;
    default:
      return;
  }
  RP2GattClient *inst = instance_for_con_handle(con_handle);
  if (inst != nullptr) {
    inst->handle_gatt_event_irq_(event_type, packet);
  }
}

void RP2GattClient::handle_gatt_event_irq_(uint8_t event_type, const uint8_t *packet) {
  switch (event_type) {
    case GATT_EVENT_MTU:
      this->enqueue_event_irq_(RP2GattEvent::MTU_EXCHANGED, 0, gatt_event_mtu_get_MTU(packet));
      break;
    case GATT_EVENT_QUERY_COMPLETE:
      this->enqueue_event_irq_(RP2GattEvent::QUERY_COMPLETE, gatt_event_query_complete_get_att_status(packet), 0);
      break;
    case GATT_EVENT_SERVICE_QUERY_RESULT: {
      if (this->arena_ == nullptr) {
        break;
      }
      if (this->service_count_ >= RP2_GATT_MAX_SERVICES) {
        this->truncated_ = true;
        break;
      }
      gatt_client_service_t service;
      gatt_event_service_query_result_get_service(packet, &service);
      auto &dst = this->arena_->services[this->service_count_];
      dst.uuid = uuid_from_btstack(service.uuid16, service.uuid128);
      dst.start_handle = service.start_group_handle;
      dst.end_handle = service.end_group_handle;
      dst.first_characteristic = 0;
      dst.characteristic_count = 0;
      this->service_count_++;
      break;
    }
    case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT: {
      if (this->arena_ == nullptr) {
        break;
      }
      if (this->char_count_ >= RP2_GATT_MAX_CHARACTERISTICS) {
        this->truncated_ = true;
        break;
      }
      gatt_client_characteristic_t characteristic;
      gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
      auto &dst = this->arena_->characteristics[this->char_count_];
      dst.uuid = uuid_from_btstack(characteristic.uuid16, characteristic.uuid128);
      dst.value_handle = characteristic.value_handle;
      dst.end_handle = characteristic.end_handle;
      dst.properties = static_cast<uint8_t>(characteristic.properties);
      dst.first_descriptor = 0;
      dst.descriptor_count = 0;
      this->char_count_++;
      break;
    }
    case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT: {
      if (this->arena_ == nullptr) {
        break;
      }
      if (this->desc_count_ >= RP2_GATT_MAX_DESCRIPTORS) {
        this->truncated_ = true;
        break;
      }
      gatt_client_characteristic_descriptor_t descriptor;
      gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &descriptor);
      auto &dst = this->arena_->descriptors[this->desc_count_];
      dst.uuid = uuid_from_btstack(descriptor.uuid16, descriptor.uuid128);
      dst.handle = descriptor.handle;
      this->desc_count_++;
      break;
    }
    case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
      // One blob per event at the reported offset; assemble into the op buffer.
      this->assemble_blob_irq_(gatt_event_long_characteristic_value_query_result_get_value_offset(packet),
                               gatt_event_long_characteristic_value_query_result_get_value(packet),
                               gatt_event_long_characteristic_value_query_result_get_value_length(packet));
      break;
    case GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
      this->assemble_blob_irq_(gatt_event_long_characteristic_descriptor_query_result_get_descriptor_offset(packet),
                               gatt_event_long_characteristic_descriptor_query_result_get_descriptor(packet),
                               gatt_event_long_characteristic_descriptor_query_result_get_descriptor_length(packet));
      break;
    case GATT_EVENT_NOTIFICATION:
      this->enqueue_notify_irq_(gatt_event_notification_get_value_handle(packet),
                                gatt_event_notification_get_value(packet),
                                gatt_event_notification_get_value_length(packet));
      break;
    case GATT_EVENT_INDICATION:
      // BTstack auto-confirms indications; deliver like a notification.
      this->enqueue_notify_irq_(gatt_event_indication_get_value_handle(packet), gatt_event_indication_get_value(packet),
                                gatt_event_indication_get_value_length(packet));
      break;
    default:
      break;
  }
}

// NOLINTBEGIN(clang-analyzer-unix.Malloc)
void RP2GattClient::assemble_blob_irq_(uint16_t offset, const uint8_t *data, uint16_t len) {
  if (offset >= RP2_GATT_MAX_ATTR_LEN) {
    return;
  }
  if (len > RP2_GATT_MAX_ATTR_LEN - offset) {
    len = RP2_GATT_MAX_ATTR_LEN - offset;
  }
  memcpy(this->op_buffer_ + offset, data, len);
  if (offset + len > this->op_len_) {
    this->op_len_ = offset + len;
  }
}

void RP2GattClient::enqueue_event_irq_(RP2GattEvent::Type type, uint8_t status, uint16_t value) {
  RP2GattEvent *event = this->event_pool_.allocate();
  if (event == nullptr) {
    this->event_queue_.increment_dropped_count();
    this->enable_loop_soon_any_context();
    return;
  }
  event->type = type;
  event->status = status;
  event->value = value;
  this->event_queue_.push(event);
  this->enable_loop_soon_any_context();
}

void RP2GattClient::enqueue_notify_irq_(uint16_t handle, const uint8_t *data, uint16_t len) {
  RP2GattNotifyEvent *event = this->notify_pool_.allocate();
  if (event == nullptr) {
    this->notify_queue_.increment_dropped_count();
    this->enable_loop_soon_any_context();
    return;
  }
  event->handle = handle;
  event->len = len > RP2_GATT_MAX_ATTR_LEN ? RP2_GATT_MAX_ATTR_LEN : len;
  memcpy(event->data, data, event->len);
  this->notify_queue_.push(event);
  this->enable_loop_soon_any_context();
}
// NOLINTEND(clang-analyzer-unix.Malloc)

// ---- Main-loop state machine ----

void RP2GattClient::loop() {
  RP2GattEvent *event;
  while ((event = this->event_queue_.pop()) != nullptr) {
    RP2GattEvent copy = *event;
    this->event_pool_.release(event);
    this->handle_event_(copy);
  }

  RP2GattNotifyEvent *notify;
  while ((notify = this->notify_queue_.pop()) != nullptr) {
    if (this->listener_ != nullptr && this->notify_subscribed_(notify->handle)) {
      this->listener_->on_notify_data(notify->handle, notify->data, notify->len);
    }
    this->notify_pool_.release(notify);
  }

  uint16_t dropped = this->event_queue_.get_and_reset_dropped_count();
  if (dropped > 0) {
    // Control events must not be lost; the connection state is no longer
    // trustworthy — recover with a forced teardown.
    ESP_LOGE(TAG, "Dropped %u GATT control events, disconnecting", dropped);
    this->disconnect();
  }
  uint16_t notify_dropped = this->notify_queue_.get_and_reset_dropped_count();
  if (notify_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u GATT notifications (queue full)", notify_dropped);
  }

  if (this->state_ == EngineState::CONNECTING || this->state_ == EngineState::MTU_EXCHANGE) {
    uint32_t now = millis();
    if (now - this->connect_started_ > CONNECT_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Connect timeout");
      if (this->state_ == EngineState::CONNECTING && this->con_handle_ == HCI_CON_HANDLE_INVALID) {
        if (!this->connect_cancel_attempted_) {
          this->connect_cancel_attempted_ = true;
          BluetoothLock lock;
          gap_connect_cancel();
          // The cancel produces a connection-complete event with a failure
          // status, which drives the normal failure path; restart the timer
          // so a lost event escalates below instead of wedging here.
          this->connect_started_ = now;
        } else {
          // The cancel's completion never arrived: reclaim the slot and the
          // scan rather than cancelling forever.
          this->fail_connection_(HCI_REASON_CONNECTION_TIMEOUT);
        }
      } else {
        // The link is up (MTU exchange stalled): tear it down properly so the
        // controller frees its side; the DISCONNECTING safety net below
        // reclaims state if the disconnection event is lost. Dropping engine
        // state without gap_disconnect would leak the live link and the
        // single GATT slot for the rest of the boot.
        this->disconnect();
      }
    }
  } else if (this->state_ == EngineState::DISCONNECTING) {
    if (millis() - this->disconnecting_started_ > DISCONNECT_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Disconnect timeout, forcing idle");
      this->handle_disconnected_(HCI_REASON_CONNECTION_TIMEOUT);
    }
  } else if (this->state_ == EngineState::READY && this->op_type_ == OpType::WRITE_CHAR_NO_RSP &&
             millis() - this->write_no_rsp_started_ > WRITE_NO_RSP_TIMEOUT_MS) {
    // The can-send window never opened; report instead of hanging the op slot.
    bool timed_out = false;
    {
      BluetoothLock lock;
      // The trampoline may have just sent it; its queued result wins.
      if (this->event_queue_.empty()) {
        this->op_type_ = OpType::NONE;
        timed_out = true;
      }
    }
    if (timed_out) {
      ESP_LOGW(TAG, "Deferred write timeout, handle=0x%04x", this->op_handle_);
      if (this->listener_ != nullptr) {
        this->listener_->on_write_result(this->op_handle_, GATT_CLIENT_BUSY);
      }
    }
  } else if (this->state_ == EngineState::IDLE || (this->state_ == EngineState::READY && !this->op_in_flight_() &&
                                                   this->event_queue_.empty() && this->notify_queue_.empty())) {
    // Nothing pending: the enqueue path re-arms the loop from any context.
    this->disable_loop();
  }
}

void RP2GattClient::handle_event_(const RP2GattEvent &event) {
  switch (event.type) {
    case RP2GattEvent::CONNECTED:
      this->handle_connected_(event.status, event.value);
      break;
    case RP2GattEvent::DISCONNECTED:
      this->handle_disconnected_(event.status);
      break;
    case RP2GattEvent::MTU_EXCHANGED:
      if (this->state_ == EngineState::MTU_EXCHANGE) {
        this->mtu_ = event.value;
        ESP_LOGD(TAG, "MTU %u", this->mtu_);
        this->state_ = EngineState::READY;
        // Scanning resumes and runs alongside the established connection.
        this->release_scan_inhibit_();
        if (this->listener_ != nullptr) {
          this->listener_->on_connection_state(true, this->mtu_, 0);
        }
      }
      break;
    case RP2GattEvent::QUERY_COMPLETE:
      this->handle_query_complete_(event.status);
      break;
    case RP2GattEvent::WRITE_NO_RSP_DONE:
      this->finish_write_no_rsp_(event.status);
      break;
    case RP2GattEvent::PAIRING_RESULT:
      if (this->listener_ != nullptr) {
        this->listener_->on_pairing_result(event.status);
      }
      break;
  }
}

void RP2GattClient::can_write_no_rsp_trampoline(void *context) {
  // BTstack context: this callback IS the can-send window, so the deferred
  // write happens here; only the result is enqueued for the main loop.
  auto *self = static_cast<RP2GattClient *>(context);
  if (self->op_type_ != OpType::WRITE_CHAR_NO_RSP) {
    return;
  }
  uint8_t status = gatt_client_write_value_of_characteristic_without_response(self->con_handle_, self->op_handle_,
                                                                              self->op_len_, self->op_buffer_);
  if ((status == GATT_CLIENT_BUSY || status == BTSTACK_ACL_BUFFERS_FULL) &&
      gatt_client_request_to_write_without_response(&self->can_write_registration_, self->con_handle_) == 0) {
    return;  // next window retries; a failed re-arm falls through as an error
  }
  self->enqueue_event_irq_(RP2GattEvent::WRITE_NO_RSP_DONE, status, 0);
}

void RP2GattClient::finish_write_no_rsp_(uint8_t status) {
  if (this->op_type_ != OpType::WRITE_CHAR_NO_RSP) {
    return;
  }
  this->op_type_ = OpType::NONE;
  if (this->listener_ != nullptr) {
    this->listener_->on_write_result(this->op_handle_, status);
  }
}

void RP2GattClient::handle_connected_(uint8_t status, uint16_t con_handle) {
  if (this->state_ != EngineState::CONNECTING) {
    return;
  }
  if (status != 0) {
    ESP_LOGW(TAG, "Connect failed, status=0x%02x", status);
    this->fail_connection_(status);
    return;
  }
  if (this->cancel_requested_) {
    // A disconnect request raced the connection complete and lost; finish
    // the teardown instead of reporting a connection nobody wants.
    this->con_handle_ = con_handle;
    this->state_ = EngineState::DISCONNECTING;
    this->disconnecting_started_ = millis();
    // No more initiating: give the radio back to the scanner during teardown.
    this->release_scan_inhibit_();
    uint8_t disc_status;
    {
      BluetoothLock lock;
      disc_status = gap_disconnect(this->con_handle_);
    }
    if (disc_status != 0) {
      this->handle_disconnected_(HCI_REASON_CONNECTION_TIMEOUT);
    }
    return;
  }
  this->con_handle_ = con_handle;
  this->state_ = EngineState::MTU_EXCHANGE;
  ESP_LOGD(TAG, "Link up, handle=0x%04x, negotiating MTU", con_handle);
  BluetoothLock lock;
  // One wildcard listener covers notifications/indications for every
  // characteristic on this connection; the CCCD writes come from the API
  // client as plain descriptor writes.
  gatt_client_listen_for_characteristic_value_updates(&this->notification_registration_,
                                                      &RP2GattClient::gatt_packet_handler, this->con_handle_, nullptr);
  // Auto MTU negotiation is disabled (see rp2040_ble enable hooks), so the
  // exchange is kicked explicitly; GATT_EVENT_MTU completes it. Without the
  // explicit kick the MTU would only be exchanged on the first GATT query,
  // which never happens on a V3_WITH_CACHE connection.
  // Both registration calls above return void (BTstack 075a078, arduino-pico
  // 6.0.0); failures surface as a missing GATT_EVENT_MTU and are reclaimed by
  // the connect timeout in loop().
  gatt_client_send_mtu_negotiation(&RP2GattClient::gatt_packet_handler, this->con_handle_);
}

void RP2GattClient::release_scan_inhibit_() {
  if (this->holds_scan_inhibit_) {
    this->holds_scan_inhibit_ = false;
    this->parent_->release_scan_inhibit();
  }
}

void RP2GattClient::fail_connection_(uint8_t reason) {
  this->cleanup_link_state_();
  this->release_scan_inhibit_();
  this->state_ = EngineState::IDLE;
  if (this->listener_ != nullptr) {
    this->listener_->on_connection_state(false, 0, reason);
  }
}

void RP2GattClient::cleanup_link_state_() {
  // Drop notifications queued behind the disconnect so they cannot emit
  // against a freed slot (address 0) on the next loop.
  RP2GattNotifyEvent *stale;
  while ((stale = this->notify_queue_.pop()) != nullptr) {
    this->notify_pool_.release(stale);
  }
  // The wildcard listener is registered on the normal connect path right
  // after con_handle_ is recorded; the cancel branch tears down before
  // registering, where stop_listening on an unregistered entry is a no-op.
  if (this->con_handle_ != HCI_CON_HANDLE_INVALID) {
    BluetoothLock lock;
    gatt_client_stop_listening_for_characteristic_value_updates(&this->notification_registration_);
  }
  this->con_handle_ = HCI_CON_HANDLE_INVALID;
  this->notify_subscription_count_ = 0;
  this->cancel_requested_ = false;
  this->op_type_ = OpType::NONE;
  this->discovery_phase_ = DiscoveryPhase::NONE;
  this->release_services();
}

void RP2GattClient::handle_disconnected_(uint8_t reason) {
  if (this->state_ == EngineState::IDLE) {
    return;
  }
  ESP_LOGD(TAG, "Disconnected, reason=0x%02x", reason);
  this->fail_connection_(reason);
}

void RP2GattClient::handle_query_complete_(uint8_t att_status) {
  // Stale completions cannot cross connections: the loop drains the whole
  // event queue every iteration, teardown resets op/discovery state, and a
  // new discovery is only issued after the new link's MTU event — which in
  // this BTstack emits no QUERY_COMPLETE (the MTU state machine is separate
  // from the query state machine). Completions with nothing in flight are
  // dropped below.
  if (this->op_type_ != OpType::NONE && this->op_type_ != OpType::WRITE_CHAR_NO_RSP) {
    OpType op = this->op_type_;
    this->op_type_ = OpType::NONE;
    if (this->listener_ == nullptr) {
      return;
    }
    switch (op) {
      case OpType::READ_CHAR:
      case OpType::READ_DESC:
        // A value that is an exact multiple of MTU - 1 ends with a trailing
        // blob request some peers refuse with INVALID_OFFSET; the read is
        // complete, not failed.
        if ((att_status == ATT_ERROR_INVALID_OFFSET || att_status == ATT_ERROR_ATTRIBUTE_NOT_LONG) &&
            this->op_len_ > 0) {
          att_status = 0;
        }
        this->listener_->on_read_result(this->op_handle_, this->op_buffer_, att_status == 0 ? this->op_len_ : 0,
                                        att_status);
        break;
      case OpType::WRITE_CHAR:
      case OpType::WRITE_DESC:
        this->listener_->on_write_result(this->op_handle_, att_status);
        break;
      default:
        break;
    }
    return;
  }
  if (this->discovery_phase_ != DiscoveryPhase::NONE) {
    this->advance_discovery_(att_status);
  }
}

// ---- Service discovery ----

int RP2GattClient::discover_services() {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  if (this->op_in_flight_()) {
    return GATT_CLIENT_IN_WRONG_STATE;
  }
  if (this->arena_ == nullptr) {
    // Transient: freed in release_services() right after the table streams
    // to the API client (mirrors Bluedroid's own per-connection GATT DB
    // lifetime on esp32). Checked: a fragmented heap must surface as a
    // stack error the proxy can report, not a device reset.
    RAMAllocator<ServiceArena> allocator(RAMAllocator<ServiceArena>::ALLOC_INTERNAL);
    this->arena_ = allocator.allocate(1);
    if (this->arena_ == nullptr) {
      ESP_LOGE(TAG, "Service table allocation failed");
      return ble_device_base::GATT_ERR_NO_MEMORY;
    }
    new (this->arena_) ServiceArena();
  }
  this->service_count_ = 0;
  this->char_count_ = 0;
  this->desc_count_ = 0;
  this->truncated_ = false;
  this->discovery_phase_ = DiscoveryPhase::SERVICES;
  BluetoothLock lock;
  uint8_t status = gatt_client_discover_primary_services(&RP2GattClient::gatt_packet_handler, this->con_handle_);
  if (status != 0) {
    this->discovery_phase_ = DiscoveryPhase::NONE;
    this->release_services();
    return status;
  }
  return 0;
}

int RP2GattClient::issue_characteristic_query_(uint16_t service_index) {
  auto &service = this->arena_->services[service_index];
  gatt_client_service_t btstack_service = {};
  btstack_service.start_group_handle = service.start_handle;
  btstack_service.end_group_handle = service.end_handle;
  service.first_characteristic = this->char_count_;
  BluetoothLock lock;
  return gatt_client_discover_characteristics_for_service(&RP2GattClient::gatt_packet_handler, this->con_handle_,
                                                          &btstack_service);
}

int RP2GattClient::issue_descriptor_query_(uint16_t char_index) {
  auto &chr = this->arena_->characteristics[char_index];
  gatt_client_characteristic_t btstack_characteristic = {};
  btstack_characteristic.value_handle = chr.value_handle;
  btstack_characteristic.end_handle = chr.end_handle;
  chr.first_descriptor = this->desc_count_;
  BluetoothLock lock;
  return gatt_client_discover_characteristic_descriptors(&RP2GattClient::gatt_packet_handler, this->con_handle_,
                                                         &btstack_characteristic);
}

void RP2GattClient::advance_discovery_(uint8_t att_status) {
  if (this->arena_ == nullptr) {
    // release_services() is publicly callable; a table freed mid-discovery
    // must end the discovery instead of dereferencing a null arena.
    this->finish_discovery_(GATT_ERR_NOT_CONNECTED);
    return;
  }
  if (att_status != 0) {
    this->finish_discovery_(att_status);
    return;
  }
  switch (this->discovery_phase_) {
    case DiscoveryPhase::SERVICES:
      if (this->service_count_ == 0) {
        this->finish_discovery_(0);
        return;
      }
      this->discovery_phase_ = DiscoveryPhase::CHARACTERISTICS;
      this->disc_service_cursor_ = 0;
      if (int err = this->issue_characteristic_query_(0); err != 0) {
        this->finish_discovery_(err);
      }
      break;
    case DiscoveryPhase::CHARACTERISTICS: {
      auto &service = this->arena_->services[this->disc_service_cursor_];
      service.characteristic_count = this->char_count_ - service.first_characteristic;
      this->disc_service_cursor_++;
      if (this->disc_service_cursor_ < this->service_count_) {
        if (int err = this->issue_characteristic_query_(this->disc_service_cursor_); err != 0) {
          this->finish_discovery_(err);
        }
        return;
      }
      if (this->char_count_ == 0) {
        this->finish_discovery_(0);
        return;
      }
      this->discovery_phase_ = DiscoveryPhase::DESCRIPTORS;
      this->disc_char_cursor_ = 0;
      if (int err = this->issue_descriptor_query_(0); err != 0) {
        this->finish_discovery_(err);
      }
      break;
    }
    case DiscoveryPhase::DESCRIPTORS: {
      auto &chr = this->arena_->characteristics[this->disc_char_cursor_];
      chr.descriptor_count = this->desc_count_ - chr.first_descriptor;
      this->disc_char_cursor_++;
      if (this->disc_char_cursor_ < this->char_count_) {
        if (int err = this->issue_descriptor_query_(this->disc_char_cursor_); err != 0) {
          this->finish_discovery_(err);
        }
        return;
      }
      this->finish_discovery_(0);
      break;
    }
    default:
      break;
  }
}

void RP2GattClient::finish_discovery_(int error) {
  this->discovery_phase_ = DiscoveryPhase::NONE;
  ESP_LOGD(TAG, "Discovery done (err=%d): %u services, %u characteristics, %u descriptors", error, this->service_count_,
           this->char_count_, this->desc_count_);
  if (error == 0 && this->truncated_) {
    // A partial table must not stream: V3 clients cache the database
    // permanently, so an incomplete one would be wrong forever.
    error = ATT_ERROR_INSUFFICIENT_RESOURCES;
  }
  if (error == 0) {
    // Discovery no longer needs the fast interval; settle into the shared
    // steady-state parameters (same lifecycle place as esp32). Status
    // discarded: BTstack fails this only for an already-gone handle.
    BluetoothLock lock;
    gap_update_connection_parameters(this->con_handle_, MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0,
                                     MEDIUM_CONN_TIMEOUT);
  }
  if (this->truncated_) {
    ESP_LOGE(TAG, "Service table truncated (device exceeds %u services / %u characteristics / %u descriptors)",
             RP2_GATT_MAX_SERVICES, RP2_GATT_MAX_CHARACTERISTICS, RP2_GATT_MAX_DESCRIPTORS);
  }
  if (error != 0) {
    this->release_services();
  }
  if (this->listener_ != nullptr) {
    this->listener_->on_service_discovery_done(error);
  }
}

ble_device_base::GattServiceTable RP2GattClient::get_service_table() {
  ble_device_base::GattServiceTable table;
  if (this->arena_ != nullptr) {
    table.services = this->arena_->services;
    table.characteristics = this->arena_->characteristics;
    table.descriptors = this->arena_->descriptors;
    table.service_count = this->service_count_;
    table.characteristic_count = this->char_count_;
    table.descriptor_count = this->desc_count_;
  }
  return table;
}

void RP2GattClient::release_services() {
  if (this->arena_ != nullptr) {
    // Under BluetoothLock so a discovery result landing in the BTstack
    // context cannot write into the arena mid-free.
    BluetoothLock lock;
    RAMAllocator<ServiceArena> allocator(RAMAllocator<ServiceArena>::ALLOC_INTERNAL);
    this->arena_->~ServiceArena();
    allocator.deallocate(this->arena_, 1);
    this->arena_ = nullptr;
  }
  this->service_count_ = 0;
  this->char_count_ = 0;
  this->desc_count_ = 0;
  this->truncated_ = false;
}

// ---- Connection control ----

int RP2GattClient::connect(uint64_t address, uint8_t addr_type) {
  if (this->is_failed()) {
    // setup() failed: nothing is registered for event routing and loop()
    // never runs, so a connect could not complete or time out.
    return GATT_ERR_NOT_CONNECTED;
  }
  if (this->state_ != EngineState::IDLE) {
    return GATT_CLIENT_IN_WRONG_STATE;
  }
  if (!this->parent_->is_active()) {
    return GATT_ERR_NOT_CONNECTED;
  }
  ble_device_base::uint64_to_mac_msb_first(address, this->peer_addr_);
  // BLE_ADDR_TYPE_* code space: bit 0 distinguishes public from random
  // (resolved RPA types 2/3 connect with the underlying kind).
  this->peer_addr_type_ = (addr_type & 1) != 0 ? BD_ADDR_TYPE_LE_RANDOM : BD_ADDR_TYPE_LE_PUBLIC;

  // Stop the shared radio's scan for the duration of the connect attempt
  // (esp32 parity: initiating and scanning contend for the radio).
  this->holds_scan_inhibit_ = true;
  this->parent_->inhibit_scan();
  this->connect_cancel_attempted_ = false;
  this->cancel_requested_ = false;
  uint8_t status;
  {
    BluetoothLock lock;
    gap_set_connection_parameters(CONN_SCAN_INTERVAL, CONN_SCAN_WINDOW, FAST_MIN_CONN_INTERVAL, FAST_MAX_CONN_INTERVAL,
                                  0, FAST_CONN_TIMEOUT, CONN_CE_MIN, CONN_CE_MAX);
    status = gap_connect(this->peer_addr_, this->peer_addr_type_);
  }
  if (status != 0) {
    ESP_LOGW(TAG, "gap_connect failed, status=0x%02x", status);
    this->release_scan_inhibit_();
    return status;
  }
  this->state_ = EngineState::CONNECTING;
  this->connect_started_ = millis();
  this->enable_loop();
  return 0;
}

int RP2GattClient::disconnect() {
  switch (this->state_) {
    case EngineState::IDLE:
      return GATT_ERR_NOT_CONNECTED;
    case EngineState::DISCONNECTING:
      return 0;  // already on its way down
    case EngineState::CONNECTING: {
      if (this->con_handle_ == HCI_CON_HANDLE_INVALID) {
        // The cancel can lose the race against a successful connection
        // complete; handle_connected_ checks this flag and finishes the
        // teardown instead of proceeding. It also counts as the one cancel
        // attempt, so a lost completion escalates on the next timeout tick.
        this->cancel_requested_ = true;
        this->connect_cancel_attempted_ = true;
        BluetoothLock lock;
        gap_connect_cancel();
        // Completion arrives as a failed connection-complete event.
        return 0;
      }
      break;
    }
    default:
      break;
  }
  uint8_t status;
  {
    BluetoothLock lock;
    status = gap_disconnect(this->con_handle_);
  }
  if (status != 0) {
    // Refused (handle already gone): complete via the event queue so the
    // listener cannot re-enter disconnect() mid-call. BluetoothLock stops
    // the IRQ producer, so this main-loop push is SPSC-safe.
    ESP_LOGW(TAG, "gap_disconnect failed, status=0x%02x", status);
    {
      BluetoothLock lock;
      this->enqueue_event_irq_(RP2GattEvent::DISCONNECTED, HCI_REASON_CONNECTION_TIMEOUT, 0);
    }
  }
  this->state_ = EngineState::DISCONNECTING;
  this->disconnecting_started_ = millis();
  // No more initiating: give the radio back to the scanner during teardown.
  this->release_scan_inhibit_();
  this->enable_loop();
  return 0;
}

// ---- GATT operations (single outstanding op) ----

int RP2GattClient::read_characteristic(uint16_t handle) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  if (this->op_in_flight_()) {
    return GATT_CLIENT_IN_WRONG_STATE;
  }
  this->op_type_ = OpType::READ_CHAR;
  this->op_handle_ = handle;
  this->op_len_ = 0;
  BluetoothLock lock;
  // Long variant: plain read first, blob continuations only past MTU - 1.
  uint8_t status = gatt_client_read_long_value_of_characteristic_using_value_handle(&RP2GattClient::gatt_packet_handler,
                                                                                    this->con_handle_, handle);
  if (status != 0) {
    this->op_type_ = OpType::NONE;
    return status;
  }
  return 0;
}

int RP2GattClient::write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  if (len > RP2_GATT_MAX_ATTR_LEN) {
    return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
  }
  if (!response) {
    // Synchronous in BTstack: the data is copied into the L2CAP buffer before
    // the call returns, and no completion event exists — synthesize one so
    // the wire behavior matches esp32 (which reports write-no-response too).
    uint8_t status;
    {
      BluetoothLock lock;
      if (this->op_type_ == OpType::WRITE_CHAR_NO_RSP) {
        // A deferred write is parked; sending now would overtake it.
        return GATT_CLIENT_BUSY;
      }
      status = gatt_client_write_value_of_characteristic_without_response(this->con_handle_, handle, len,
                                                                          const_cast<uint8_t *>(data));
      // BTSTACK_ACL_BUFFERS_FULL is the same transient flow control one layer
      // down (L2CAP), so it defers identically.
      if (status == GATT_CLIENT_BUSY || status == BTSTACK_ACL_BUFFERS_FULL) {
        if (this->op_in_flight_()) {
          // The op buffer is owned; bounce the busy to the caller as before.
          return status;
        }
        // Stash the payload and send from the can-send callback.
        memcpy(this->op_buffer_, data, len);
        this->op_type_ = OpType::WRITE_CHAR_NO_RSP;
        this->op_handle_ = handle;
        this->op_len_ = len;
        this->write_no_rsp_started_ = millis();
        this->can_write_registration_.callback = &RP2GattClient::can_write_no_rsp_trampoline;
        this->can_write_registration_.context = this;
        uint8_t req = gatt_client_request_to_write_without_response(&this->can_write_registration_, this->con_handle_);
        if (req != 0 && req != ERROR_CODE_COMMAND_DISALLOWED) {
          this->op_type_ = OpType::NONE;
          return req;
        }
        // COMMAND_DISALLOWED = still armed from a timed-out deferral; that
        // registration sends the newly parked payload. Keep the loop running
        // so the deadline below can fire on a stalled link.
        this->enable_loop();
        return 0;
      }
    }
    if (status == 0 && this->listener_ != nullptr) {
      this->listener_->on_write_result(handle, 0);
    }
    return status;
  }
  if (this->op_in_flight_()) {
    return GATT_CLIENT_IN_WRONG_STATE;
  }
  // BTstack keeps the caller's pointer until the request is sent; the payload
  // must live in engine-owned storage across the async operation.
  memcpy(this->op_buffer_, data, len);
  this->op_type_ = OpType::WRITE_CHAR;
  this->op_handle_ = handle;
  BluetoothLock lock;
  uint8_t status;
  if (len <= this->mtu_ - 3) {
    status = gatt_client_write_value_of_characteristic(&RP2GattClient::gatt_packet_handler, this->con_handle_, handle,
                                                       len, this->op_buffer_);
  } else {
    status = gatt_client_write_long_value_of_characteristic(&RP2GattClient::gatt_packet_handler, this->con_handle_,
                                                            handle, len, this->op_buffer_);
  }
  if (status != 0) {
    this->op_type_ = OpType::NONE;
    return status;
  }
  return 0;
}

int RP2GattClient::read_descriptor(uint16_t handle) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  if (this->op_in_flight_()) {
    return GATT_CLIENT_IN_WRONG_STATE;
  }
  this->op_type_ = OpType::READ_DESC;
  this->op_handle_ = handle;
  this->op_len_ = 0;
  BluetoothLock lock;
  uint8_t status = gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(
      &RP2GattClient::gatt_packet_handler, this->con_handle_, handle);
  if (status != 0) {
    this->op_type_ = OpType::NONE;
    return status;
  }
  return 0;
}

int RP2GattClient::write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  if (this->op_in_flight_()) {
    return GATT_CLIENT_IN_WRONG_STATE;
  }
  if (len > RP2_GATT_MAX_ATTR_LEN) {
    return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
  }
  memcpy(this->op_buffer_, data, len);
  this->op_type_ = OpType::WRITE_DESC;
  this->op_handle_ = handle;
  BluetoothLock lock;
  uint8_t status = gatt_client_write_characteristic_descriptor_using_descriptor_handle(
      &RP2GattClient::gatt_packet_handler, this->con_handle_, handle, len, this->op_buffer_);
  if (status != 0) {
    this->op_type_ = OpType::NONE;
    return status;
  }
  return 0;
}

int RP2GattClient::pair() {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  BluetoothLock lock;
  sm_request_pairing(this->con_handle_);  // void API; completion via SM events
  return 0;
}

int RP2GattClient::notify_characteristic(uint16_t handle, bool enable) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  // The CCCD write arrives separately as a descriptor write (V3 semantics);
  // this call only gates local delivery via the subscription list.
  if (enable) {
    if (!this->notify_subscribed_(handle)) {
      if (this->notify_subscription_count_ >= RP2_GATT_MAX_NOTIFY_SUBSCRIPTIONS) {
        return GATT_ERR_NO_MEMORY;
      }
      this->notify_subscriptions_[this->notify_subscription_count_++] = handle;
    }
  } else {
    for (uint8_t i = 0; i < this->notify_subscription_count_; i++) {
      if (this->notify_subscriptions_[i] == handle) {
        this->notify_subscriptions_[i] = this->notify_subscriptions_[--this->notify_subscription_count_];
        break;
      }
    }
  }
  if (this->listener_ != nullptr) {
    this->listener_->on_notify_state(handle, enable, 0);
  }
  return 0;
}

bool RP2GattClient::notify_subscribed_(uint16_t handle) const {
  for (uint8_t i = 0; i < this->notify_subscription_count_; i++) {
    if (this->notify_subscriptions_[i] == handle) {
      return true;
    }
  }
  return false;
}

int RP2GattClient::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                            uint16_t timeout) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  BluetoothLock lock;
  return gap_update_connection_parameters(this->con_handle_, min_interval, max_interval, latency, timeout);
}

conn_err_t unpair_device(uint64_t address) {
  uint8_t mac[6];
  ble_device_base::uint64_to_mac_msb_first(address, mac);
  bool found = false;
  BluetoothLock lock;
  // Exhaustive: the db keys on (type, address), so stale entries can share
  // the same address bytes under different types.
  for (int i = 0; i < le_device_db_max_count(); i++) {
    int addr_type = 0;
    bd_addr_t addr;
    le_device_db_info(i, &addr_type, addr, nullptr);
    if (addr_type != BD_ADDR_TYPE_UNKNOWN && memcmp(addr, mac, sizeof(bd_addr_t)) == 0) {
      le_device_db_remove(i);
      found = true;
    }
  }
  if (found) {
    return CONN_OK;
  }
  // No bond for this address; the shared error domain has no closer code
  // (esp32 parity: its remove-bond call also errors for an unknown address).
  return GATT_NOT_CONNECTED;
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_RP2040_BLE && USE_BLE_GATT_CLIENT
