// RP2 (Pico W / Pico 2 W) GATT client backend over BTstack.
//
// The build's ble_device_base::BLEGattConnection backend (bound by alias in
// bluetooth_connection_gatt_backend.h) for the hub BluetoothConnection wrapper. BTstack packet handlers run in the
// CYW43 async-context low-priority IRQ (or on the main-loop stack during BluetoothLock release), so handlers only copy
// into per-instance lock-free queues/storage; loop() drains them and drives the state machine. Every BTstack call
// issued from the main loop is wrapped in BluetoothLock.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_RP2040_BLE) && defined(USE_BLE_GATT_CLIENT)

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/components/rp2040_ble/rp2040_ble.h"
#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

#include <btstack.h>

#include <array>
#include <cstdint>

namespace esphome::bluetooth_connection {

// Caps for the transient service table. Sized generously for real devices
// (typical peripherals expose < 8 services / < 30 characteristics); a peer
// exceeding a cap fails discovery with INSUFFICIENT_RESOURCES rather than
// streaming an incomplete database a V3 client would cache permanently.
static constexpr uint16_t RP2_GATT_MAX_SERVICES = 16;
static constexpr uint16_t RP2_GATT_MAX_CHARACTERISTICS = 96;
static constexpr uint16_t RP2_GATT_MAX_DESCRIPTORS = 96;

// Concurrent notify subscriptions per connection (enable fails with
// GATT_ERR_NO_MEMORY when exceeded; real clients subscribe to a handful).
static constexpr uint8_t RP2_GATT_MAX_NOTIFY_SUBSCRIPTIONS = 16;

// ATT spec maximum attribute value length; bounds the op buffer and
// notification payloads.
static constexpr uint16_t RP2_GATT_MAX_ATTR_LEN = 512;

// Control events from the BTstack handlers to loop().
struct RP2GattEvent {
  enum Type : uint8_t {
    CONNECTED,          // status + con_handle (value)
    DISCONNECTED,       // status = HCI reason
    MTU_EXCHANGED,      // value = negotiated MTU
    QUERY_COMPLETE,     // status = ATT status of the finished query
    WRITE_NO_RSP_DONE,  // status = result of the deferred write
    PAIRING_RESULT,     // status = SM pairing status (0 = bonded)
  };
  Type type;
  uint8_t status;
  uint16_t value;
  void release() {}
};

// One notification/indication from the peer.
struct RP2GattNotifyEvent {
  uint16_t handle;
  uint16_t len;
  uint8_t data[RP2_GATT_MAX_ATTR_LEN];
  void release() {}
};

static constexpr uint8_t RP2_GATT_EVENT_QUEUE_SIZE = 8;
// Depth 4: the queue is drained every main-loop iteration and each slot is a
// full 512 B ATT payload, so depth buys burst tolerance at ~516 B per slot.
static constexpr uint8_t RP2_GATT_NOTIFY_QUEUE_SIZE = 4;

class RP2GattClient final : public Component,
                            public Parented<rp2040_ble::RP2040BLE>
#ifdef USE_OTA_STATE_LISTENER
    ,
                            public ota::OTAGlobalStateListener
#endif
{
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_listener(ble_device_base::GattClientListener *listener) { this->listener_ = listener; }

  // ---- ble_device_base::BLEGattConnection contract ----
  int connect(uint64_t address, uint8_t addr_type);
  int gatt_disconnect();
  // Teardown starts inside gatt_disconnect() on this backend; nothing is
  // ever scheduled, so there is nothing to cancel.
  bool cancel_gatt_disconnect() { return false; }
  int discover_services();
  int read_characteristic(uint16_t handle);
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response);
  int read_descriptor(uint16_t handle);
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len);
  int notify_characteristic(uint16_t handle, bool enable);
  int pair();
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
  ble_device_base::GattServiceTable get_service_table();
  // Cached connections initiate at MEDIUM parameters (esp32 parity); FAST is
  // reserved for the discovery phase of uncached connects.
  void set_connection_type(ble_device_base::ConnectionType ct) { this->connection_type_ = ct; }
  void release_services();

#ifdef USE_OTA_STATE_LISTENER
  // Drop the connection while an OTA runs (esp32 parity): an active link
  // competes with the transfer for the shared radio.
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif

 protected:
  // Link/engine state. Discovery and GATT ops have their own cursors below —
  // the link stays READY while they run.
  enum class EngineState : uint8_t {
    IDLE,
    CONNECT_PENDING,  // queued: another engine owns the stack-wide create-connection
    CONNECTING,       // gap_connect issued, waiting for connection complete
    MTU_EXCHANGE,     // link up, waiting for GATT_EVENT_MTU
    READY,            // on_connection_state(true) delivered
    DISCONNECTING,
  };

  enum class DiscoveryPhase : uint8_t { NONE, SERVICES, CHARACTERISTICS, DESCRIPTORS };

  enum class OpType : uint8_t { NONE, READ_CHAR, WRITE_CHAR, WRITE_CHAR_NO_RSP, READ_DESC, WRITE_DESC };

  // The whole table in one transient allocation (RAMAllocator, checked),
  // freed after streaming.
  struct ServiceArena {
    ble_device_base::GattService services[RP2_GATT_MAX_SERVICES];
    ble_device_base::GattCharacteristic characteristics[RP2_GATT_MAX_CHARACTERISTICS];
    ble_device_base::GattDescriptor descriptors[RP2_GATT_MAX_DESCRIPTORS];
  };

  // BTstack packet handlers (IRQ context: copy-and-enqueue only).
  static void hci_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size);
  static void gatt_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size);
  static void sm_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size);
  static RP2GattClient *instance_for_con_handle(hci_con_handle_t con_handle);

  void handle_gatt_event_irq_(uint8_t event_type, const uint8_t *packet);
  void enqueue_event_irq_(RP2GattEvent::Type type, uint8_t status, uint16_t value);
  void enqueue_notify_irq_(uint16_t handle, const uint8_t *data, uint16_t len);
  void assemble_blob_irq_(uint16_t offset, const uint8_t *data, uint16_t len);

  // Main-loop state machine.
  void handle_event_(const RP2GattEvent &event);
  void handle_connected_(uint8_t status, uint16_t con_handle);
  void handle_disconnected_(uint8_t reason);
  void handle_query_complete_(uint8_t att_status);
  void advance_discovery_(uint8_t att_status);
  int issue_characteristic_query_(uint16_t service_index);
  int issue_descriptor_query_(uint16_t char_index);
  void finish_discovery_(int error);
  void fail_connection_(uint8_t reason);
  int try_gap_connect_();
  void cleanup_link_state_();
  bool notify_subscribed_(uint16_t handle) const;
  static void can_write_no_rsp_trampoline(void *context);
  void finish_write_no_rsp_(uint8_t status);
  void release_scan_inhibit_();
  bool op_in_flight_() const {
    return this->op_type_ != OpType::NONE || this->discovery_phase_ != DiscoveryPhase::NONE;
  }

  // Group 1: containers / large storage
  ble_device_base::GattClientListener *listener_{nullptr};
  ServiceArena *arena_{nullptr};
  esphome::LockFreeQueue<RP2GattEvent, RP2_GATT_EVENT_QUEUE_SIZE> event_queue_;
  esphome::EventPool<RP2GattEvent, RP2_GATT_EVENT_QUEUE_SIZE - 1> event_pool_;
  esphome::LockFreeQueue<RP2GattNotifyEvent, RP2_GATT_NOTIFY_QUEUE_SIZE> notify_queue_;
  esphome::EventPool<RP2GattNotifyEvent, RP2_GATT_NOTIFY_QUEUE_SIZE - 1> notify_pool_;

  // Shared buffer for the single outstanding GATT op: write payloads (BTstack
  // keeps the caller's pointer until the request is sent) and read results
  // (written from the handler, read after QUERY_COMPLETE is drained).
  uint8_t op_buffer_[RP2_GATT_MAX_ATTR_LEN];

  // BTstack registrations
  gatt_client_notification_t notification_registration_{};
  btstack_context_callback_registration_t can_write_registration_{};

  // Group 3: 4-byte types
  uint32_t connect_started_{0};
  uint32_t connect_retry_ms_{0};  // last CONNECT_PENDING gap_connect attempt
  uint32_t disconnecting_started_{0};
  uint32_t write_no_rsp_started_{0};
  // Unscoped C enum, so int-sized: lives with the 4-byte members to keep the
  // padding at the tail.
  bd_addr_type_t peer_addr_type_{BD_ADDR_TYPE_LE_PUBLIC};

  // Group 4: 2-byte types (table counters written from the handler during
  // discovery, read from the main loop after the phase's QUERY_COMPLETE)
  hci_con_handle_t con_handle_{HCI_CON_HANDLE_INVALID};
  uint16_t mtu_{ble_device_base::DEFAULT_ATT_MTU};
  uint16_t op_handle_{0};
  uint16_t op_len_{0};
  uint16_t service_count_{0};
  uint16_t char_count_{0};
  uint16_t desc_count_{0};
  uint16_t disc_service_cursor_{0};
  uint16_t disc_char_cursor_{0};

  // Group 5: arrays / 1-byte types
  // Subscribed notify handles; the loop() drain filters the wildcard
  // listener's deliveries on this list (esp32 parity for enable=false).
  std::array<uint16_t, RP2_GATT_MAX_NOTIFY_SUBSCRIPTIONS> notify_subscriptions_{};
  uint8_t notify_subscription_count_{0};
  uint8_t engine_index_{0};  // position in instances[]; tags log lines per slot
  bd_addr_t peer_addr_{};    // MSB-first, as gap_connect expects
  ble_device_base::ConnectionType connection_type_{ble_device_base::ConnectionType::V3_WITHOUT_CACHE};
  EngineState state_{EngineState::IDLE};
  DiscoveryPhase discovery_phase_{DiscoveryPhase::NONE};
  OpType op_type_{OpType::NONE};
  bool truncated_{false};
  // One cancel attempt per connect: the second timeout escalates to failure.
  bool connect_cancel_attempted_{false};
  // A disconnect request raced an in-flight connect; finish teardown on link-up.
  bool cancel_requested_{false};
  // This engine's own hold on the shared scan inhibit, so the pairing stays
  // one-to-one per connection even with multiple slots.
  bool holds_scan_inhibit_{false};

  // Instance registry for routing BTstack events (IRQ context) to engines.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static RP2GattClient *instances[ESPHOME_BLE_GATT_CLIENT_COUNT];
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static uint8_t instance_count;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static btstack_packet_callback_registration_t hci_event_registration;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static btstack_packet_callback_registration_t sm_event_registration;
  // The engine whose gap_connect is in flight: BTstack allows one outgoing LE
  // create-connection stack-wide, and gap_connect_cancel is global, so only
  // the owner may cancel. Written under BluetoothLock from the main loop,
  // cleared in the BTstack context when the procedure resolves.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static RP2GattClient *connect_owner;
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_RP2040_BLE && USE_BLE_GATT_CLIENT
