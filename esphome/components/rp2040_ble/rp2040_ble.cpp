#include "rp2040_ble.h"

#ifdef USE_RP2040_BLE

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <BluetoothLock.h>

#include <cstring>

namespace esphome::rp2040_ble {

static const char *const TAG = "rp2040_ble";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
RP2040BLE *global_ble = nullptr;

void RP2040BLE::setup() {
  global_ble = this;

  // Pre-create every pool entry so the packet handler's allocate() is always a
  // free-list pop — the IRQ path must never reach malloc() (the newlib malloc
  // lock is not IRQ-safe). Deliberately
  // unconditional: warming lazily on the first scan would move the allocations
  // after setup, and doing it here keeps the pool's RAM cost visible at
  // startup instead of appearing once scanning begins. On an incomplete warm,
  // refuse to run instead (the stack is never enabled, so the packet handler
  // cannot fire).
  if (!this->report_pool_.warm()) {
    ESP_LOGE(TAG, "Scan report pool warm-up failed");
    this->mark_failed();
    return;
  }

  if (this->enable_on_boot_) {
    this->enable();
  } else {
    this->state_ = BLEComponentState::DISABLED;
  }
}

void RP2040BLE::enable() {
  if (this->state_ == BLEComponentState::ACTIVE || this->state_ == BLEComponentState::ENABLING) {
    return;
  }

  ESP_LOGD(TAG, "Enabling BLE...");
  this->state_ = BLEComponentState::ENABLING;
  this->active_logged_ = false;

  if (!this->btstack_initialized_) {
    // Serialize with the BTstack background worker while wiring the stack up
    // (arduino-pico's BluetoothHCI::install() takes the same lock here).
    BluetoothLock lock;

    // BTstack init functions are not idempotent — only call once
    l2cap_init();
    sm_init();

#ifdef USE_BLE_GATT_CLIENT
    gatt_client_init();
    // The GATT engine kicks the MTU exchange explicitly right after a
    // connection completes (auto negotiation would only run on the first
    // query, which a with-cache connection never issues).
    gatt_client_mtu_enable_auto_negotiation(0);
    // Just-works security for peripheral-initiated pairing.
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
#endif

    this->hci_event_callback_registration_.callback = &RP2040BLE::packet_handler;
    hci_add_event_handler(&this->hci_event_callback_registration_);

    this->sm_event_callback_registration_.callback = &RP2040BLE::packet_handler;
    sm_add_event_handler(&this->sm_event_callback_registration_);

    this->btstack_initialized_ = true;
  }

  BluetoothLock lock;
  hci_power_control(HCI_POWER_ON);
}

void RP2040BLE::disable() {
  if (this->state_ == BLEComponentState::DISABLED || this->state_ == BLEComponentState::STATE_OFF) {
    return;
  }

  ESP_LOGD(TAG, "Disabling BLE...");
  this->state_ = BLEComponentState::DISABLING;

  {
    BluetoothLock lock;
    hci_power_control(HCI_POWER_OFF);
  }

  this->state_ = BLEComponentState::DISABLED;
  ESP_LOGD(TAG, "BLE disabled");
}

void RP2040BLE::loop() {
  if (this->state_ == BLEComponentState::ACTIVE && !this->active_logged_) {
    this->active_logged_ = true;
    // The controller address becomes readable once HCI reaches WORKING.
    // bd_addr_to_str() formats into a BTstack-internal static buffer, so both
    // calls stay under the lock like every other BTstack call from the loop.
    BluetoothLock lock;
    gap_local_bd_addr(this->ble_mac_);
    ESP_LOGI(TAG, "BLE active (MAC %s)", bd_addr_to_str(this->ble_mac_));
  }

  // Drain the lock-free ring filled by the BTstack packet handler; all
  // per-report work runs here on the main loop, then the report returns to
  // the pool.
  BLEScanReport *report = this->report_queue_.pop();
  if (report == nullptr)
    return;
  do {
#ifdef RP2040_BLE_SCAN_LISTENER_COUNT
    for (auto *listener : this->scan_listeners_)
      listener->on_scan_report(*report);
#endif
    this->report_pool_.release(report);
  } while ((report = this->report_queue_.pop()) != nullptr);

  uint16_t dropped = this->report_queue_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u scan reports (queue full)", dropped);
  }
}

static const char *state_to_str(BLEComponentState state) {
  switch (state) {
    case BLEComponentState::STATE_OFF:
      return "OFF";
    case BLEComponentState::ENABLING:
      return "ENABLING";
    case BLEComponentState::ACTIVE:
      return "ACTIVE";
    case BLEComponentState::DISABLING:
      return "DISABLING";
    case BLEComponentState::DISABLED:
      return "DISABLED";
    default:
      return "UNKNOWN";
  }
}

void RP2040BLE::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RP2040 BLE:\n"
                "  Enable on boot: %s\n"
                "  State: %s",
                YESNO(this->enable_on_boot_), state_to_str(this->state_));
}

float RP2040BLE::get_setup_priority() const { return setup_priority::BLUETOOTH; }

void RP2040BLE::packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
  if (global_ble == nullptr) {
    return;
  }

  if (type != HCI_EVENT_PACKET) {
    return;
  }

  uint8_t event_type = hci_event_packet_get_type(packet);

  switch (event_type) {
    case BTSTACK_EVENT_STATE: {
      uint8_t state = btstack_event_state_get_state(packet);
      if (state == HCI_STATE_WORKING && global_ble->state_ == BLEComponentState::ENABLING) {
        global_ble->state_ = BLEComponentState::ACTIVE;
      }
      break;
    }
    case GAP_EVENT_ADVERTISING_REPORT: {
      // Runs in the CYW43 async-context worker (low-priority IRQ), NOT the
      // ESPHome main loop: bounded copy into the lock-free queue only.
      bd_addr_t addr;  // accessor returns printable (MSB-first) order
      gap_event_advertising_report_get_address(packet, addr);
      uint8_t mac_lsb[MAC_ADDRESS_SIZE];
      reverse_bd_addr(addr, mac_lsb);  // LSB-first, the BLE convention consumers expect
      global_ble->enqueue_scan_report_(mac_lsb, static_cast<int8_t>(gap_event_advertising_report_get_rssi(packet)),
                                       gap_event_advertising_report_get_address_type(packet),
                                       gap_event_advertising_report_get_advertising_event_type(packet),
                                       gap_event_advertising_report_get_data(packet),
                                       gap_event_advertising_report_get_data_length(packet));
      break;
    }
    default:
      break;
  }
}

// The analyzer traces a leak on the failed-push path, which cannot happen: the
// pool is sized to the queue capacity (SIZE-1), so allocate() returns nullptr
// before push() can find the ring full.
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
void RP2040BLE::enqueue_scan_report_(const uint8_t *mac_lsb_first, int8_t rssi, uint8_t addr_type,
                                     uint8_t adv_event_type, const uint8_t *data, uint16_t data_len) {
  BLEScanReport *report = this->report_pool_.allocate();
  if (report == nullptr) {
    // Pool exhausted — the queue is full; count and drop.
    this->report_queue_.increment_dropped_count();
    return;
  }
  memcpy(report->mac, mac_lsb_first, MAC_ADDRESS_SIZE);
  report->rssi = rssi;
  report->addr_type = addr_type;
  report->adv_event_type = adv_event_type;
  report->data_len =
      (data_len <= sizeof(report->data)) ? static_cast<uint8_t>(data_len) : static_cast<uint8_t>(sizeof(report->data));
  memcpy(report->data, data, report->data_len);
  this->report_queue_.push(report);
}
// NOLINTEND(clang-analyzer-unix.Malloc)

void RP2040BLE::get_mac_msb_first(uint8_t out[MAC_ADDRESS_SIZE]) const {
  memcpy(out, this->ble_mac_, MAC_ADDRESS_SIZE);
}

bool RP2040BLE::scan_start(uint16_t interval, uint16_t window, bool active) {
  if (!this->is_active()) {
    // Power control stays with the user (enable_on_boot or an explicit
    // enable() call) — auto-enabling here would defeat enable_on_boot: false
    // the moment a tracker retries. Callers retry until the stack is up.
    return false;
  }
#ifdef USE_BLE_GATT_CLIENT
  this->scan_interval_ = interval;
  this->scan_window_ = window;
  this->scan_active_mode_ = active;
  this->scan_desired_ = true;
  if (this->scan_inhibit_count_ > 0) {
    // A connect attempt owns the radio; the scan starts physically when the
    // inhibit is released. Report success — the controller will run it.
    ESP_LOGV(TAG, "Scan start deferred (connect in progress)");
    return true;
  }
#endif
  // Serialize with the BTstack background worker (arduino-pico's BluetoothHCI
  // takes the same lock around its gap_* calls).
  BluetoothLock lock;
  gap_set_scan_params(active ? 1 : 0, interval, window, 0 /* accept all */);
  gap_start_scan();
  return true;
}

void RP2040BLE::scan_stop() {
#ifdef USE_BLE_GATT_CLIENT
  this->scan_desired_ = false;
#endif
  if (!this->is_active()) {
    return;  // nothing can be scanning on a stack that is not up
  }
  BluetoothLock lock;
  gap_stop_scan();
}

#ifdef USE_BLE_GATT_CLIENT
void RP2040BLE::inhibit_scan() {
  if (this->scan_inhibit_count_++ != 0) {
    return;  // another connect attempt already owns the radio
  }
  if (this->scan_desired_ && this->is_active()) {
    BluetoothLock lock;
    gap_stop_scan();
  }
}

void RP2040BLE::release_scan_inhibit() {
  if (this->scan_inhibit_count_ == 0) {
    return;
  }
  this->scan_inhibit_count_--;
  if (this->scan_inhibit_count_ != 0) {
    return;
  }
  if (this->scan_desired_) {
    // One physical-start path: scan_start re-applies the remembered params.
    this->scan_start(this->scan_interval_, this->scan_window_, this->scan_active_mode_);
  }
}
#endif  // USE_BLE_GATT_CLIENT

}  // namespace esphome::rp2040_ble

#endif  // USE_RP2040_BLE
