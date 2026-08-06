#include "bluetooth_connection_rp2.h"

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

// Engine-owned timeouts: BTstack has a 30 s ATT transaction timeout but no
// connect timeout — a stuck LE_CONNECTING both blocks future gap_connect calls
// and keeps the scan inhibited, so the engine cancels after 20 s. The
// disconnect timeout mirrors the esp32 CLOSE_EVT safety net.
static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
static constexpr uint32_t DISCONNECT_TIMEOUT_MS = 10000;

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
RP2GattClient *RP2GattClient::instances_[ESPHOME_BLE_GATT_CLIENT_COUNT] = {};
uint8_t RP2GattClient::instance_count_ = 0;
btstack_packet_callback_registration_t RP2GattClient::hci_event_registration_ = {};
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
  if (this->instance_count_ < ESPHOME_BLE_GATT_CLIENT_COUNT) {
    instances_[instance_count_++] = this;
  }

  // One HCI event handler for all engine instances (BTstack supports multiple
  // registrations, so rp2040_ble's own handler is unaffected).
  if (hci_event_registration_.callback == nullptr) {
    BluetoothLock lock;
    hci_event_registration_.callback = &RP2GattClient::hci_packet_handler;
    hci_add_event_handler(&hci_event_registration_);
  }

  this->disable_loop();
}

float RP2GattClient::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void RP2GattClient::dump_config() { ESP_LOGCONFIG(TAG, "RP2 GATT client (BTstack)"); }

// ---- IRQ-context handlers: copy-and-enqueue only ----

RP2GattClient *RP2GattClient::instance_for_con_handle_(hci_con_handle_t con_handle) {
  for (uint8_t i = 0; i < instance_count_; i++) {
    if (instances_[i]->con_handle_ == con_handle) {
      return instances_[i];
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
      for (uint8_t i = 0; i < instance_count_; i++) {
        RP2GattClient *inst = instances_[i];
        if (inst->state_ == EngineState::CONNECTING && memcmp(inst->peer_addr_, peer, sizeof(bd_addr_t)) == 0) {
          inst->enqueue_event_irq_(RP2GattEvent::CONNECTED, status, con_handle);
          break;
        }
      }
      break;
    }
    case HCI_EVENT_DISCONNECTION_COMPLETE: {
      hci_con_handle_t con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
      RP2GattClient *inst = instance_for_con_handle_(con_handle);
      if (inst == nullptr) {
        // The main loop may not have recorded the handle yet (the CONNECTED
        // event is still queued); route to the instance that is connecting so
        // an accept-then-drop peer cannot leave the engine on a dead handle.
        for (uint8_t i = 0; i < instance_count_; i++) {
          RP2GattClient *candidate = instances_[i];
          if (candidate->con_handle_ == HCI_CON_HANDLE_INVALID && candidate->state_ != EngineState::IDLE) {
            inst = candidate;
            break;
          }
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
    case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
      con_handle = gatt_event_characteristic_value_query_result_get_handle(packet);
      break;
    case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
      con_handle = gatt_event_long_characteristic_value_query_result_get_handle(packet);
      break;
    case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
      con_handle = gatt_event_characteristic_descriptor_query_result_get_handle(packet);
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
  RP2GattClient *inst = instance_for_con_handle_(con_handle);
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
    case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT: {
      uint16_t len = gatt_event_characteristic_value_query_result_get_value_length(packet);
      if (len > RP2_GATT_MAX_ATTR_LEN) {
        len = RP2_GATT_MAX_ATTR_LEN;
      }
      memcpy(this->op_buffer_, gatt_event_characteristic_value_query_result_get_value(packet), len);
      this->op_len_ = len;
      break;
    }
    case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT: {
      uint16_t offset = gatt_event_long_characteristic_value_query_result_get_value_offset(packet);
      uint16_t len = gatt_event_long_characteristic_value_query_result_get_value_length(packet);
      if (offset >= RP2_GATT_MAX_ATTR_LEN) {
        break;
      }
      if (offset + len > RP2_GATT_MAX_ATTR_LEN) {
        len = RP2_GATT_MAX_ATTR_LEN - offset;
      }
      memcpy(this->op_buffer_ + offset, gatt_event_long_characteristic_value_query_result_get_value(packet), len);
      if (offset + len > this->op_len_) {
        this->op_len_ = offset + len;
      }
      break;
    }
    case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT: {
      uint16_t len = gatt_event_characteristic_descriptor_query_result_get_descriptor_length(packet);
      if (len > RP2_GATT_MAX_ATTR_LEN) {
        len = RP2_GATT_MAX_ATTR_LEN;
      }
      memcpy(this->op_buffer_, gatt_event_characteristic_descriptor_query_result_get_descriptor(packet), len);
      this->op_len_ = len;
      break;
    }
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
void RP2GattClient::enqueue_event_irq_(RP2GattEvent::Type type, uint8_t status, uint16_t value) {
  RP2GattEvent *event = this->event_pool_.allocate();
  if (event == nullptr) {
    this->event_queue_.increment_dropped_count();
    return;
  }
  event->type = type;
  event->status = status;
  event->value = value;
  this->event_queue_.push(event);
}

void RP2GattClient::enqueue_notify_irq_(uint16_t handle, const uint8_t *data, uint16_t len) {
  RP2GattNotifyEvent *event = this->notify_pool_.allocate();
  if (event == nullptr) {
    this->notify_queue_.increment_dropped_count();
    return;
  }
  event->handle = handle;
  event->len = len > RP2_GATT_MAX_ATTR_LEN ? RP2_GATT_MAX_ATTR_LEN : len;
  memcpy(event->data, data, event->len);
  this->notify_queue_.push(event);
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
    if (this->listener_ != nullptr) {
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
  } else if (this->state_ == EngineState::IDLE) {
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
  // The wildcard listener is registered exactly while a connection handle
  // exists (both are set together in handle_connected_).
  if (this->con_handle_ != HCI_CON_HANDLE_INVALID) {
    BluetoothLock lock;
    gatt_client_stop_listening_for_characteristic_value_updates(&this->notification_registration_);
  }
  this->con_handle_ = HCI_CON_HANDLE_INVALID;
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
  if (this->op_type_ != OpType::NONE) {
    OpType op = this->op_type_;
    this->op_type_ = OpType::NONE;
    if (this->listener_ == nullptr) {
      return;
    }
    switch (op) {
      case OpType::READ_CHAR:
      case OpType::READ_DESC:
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
      return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
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
  if (error == 0) {
    // Discovery no longer needs the fast interval; settle into the shared
    // steady-state parameters (same lifecycle place as esp32).
    BluetoothLock lock;
    gap_update_connection_parameters(this->con_handle_, MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0,
                                     MEDIUM_CONN_TIMEOUT);
  }
  if (error == 0 && this->truncated_) {
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
  {
    BluetoothLock lock;
    gap_disconnect(this->con_handle_);
  }
  this->state_ = EngineState::DISCONNECTING;
  this->disconnecting_started_ = millis();
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
  uint8_t status = gatt_client_read_value_of_characteristic_using_value_handle(&RP2GattClient::gatt_packet_handler,
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
      status = gatt_client_write_value_of_characteristic_without_response(this->con_handle_, handle, len,
                                                                          const_cast<uint8_t *>(data));
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
  uint8_t status = gatt_client_read_characteristic_descriptor_using_descriptor_handle(
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

int RP2GattClient::notify_characteristic(uint16_t handle, bool enable) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  // The per-connection wildcard listener is registered at connect time and
  // covers every characteristic; the CCCD write arrives from the API client
  // as a plain descriptor write (V3 semantics, same as esp32). Registration
  // is therefore bookkeeping only — confirm it so the client gets its
  // BluetoothGATTNotifyResponse.
  if (this->listener_ != nullptr) {
    this->listener_->on_notify_state(handle, enable, 0);
  }
  return 0;
}

int RP2GattClient::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                            uint16_t timeout) {
  if (this->state_ != EngineState::READY) {
    return GATT_ERR_NOT_CONNECTED;
  }
  BluetoothLock lock;
  return gap_update_connection_parameters(this->con_handle_, min_interval, max_interval, latency, timeout);
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_RP2040_BLE && USE_BLE_GATT_CLIENT
