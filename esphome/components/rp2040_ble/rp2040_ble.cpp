#include "rp2040_ble.h"

#ifdef USE_RP2040_BLE

#include "esphome/core/log.h"

namespace esphome::rp2040_ble {

static const char *const TAG = "rp2040_ble";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
RP2040BLE *global_ble = nullptr;

void RP2040BLE::setup() {
  global_ble = this;

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
    // BTstack init functions are not idempotent — only call once
    l2cap_init();
    sm_init();

    this->hci_event_callback_registration_.callback = &RP2040BLE::packet_handler_;
    hci_add_event_handler(&this->hci_event_callback_registration_);

    this->sm_event_callback_registration_.callback = &RP2040BLE::packet_handler_;
    sm_add_event_handler(&this->sm_event_callback_registration_);

    this->btstack_initialized_ = true;
  }

  hci_power_control(HCI_POWER_ON);
}

void RP2040BLE::disable() {
  if (this->state_ == BLEComponentState::DISABLED || this->state_ == BLEComponentState::OFF) {
    return;
  }

  ESP_LOGD(TAG, "Disabling BLE...");
  this->state_ = BLEComponentState::DISABLING;

  hci_power_control(HCI_POWER_OFF);

  this->state_ = BLEComponentState::DISABLED;
  ESP_LOGD(TAG, "BLE disabled");
}

void RP2040BLE::loop() {
  if (this->state_ == BLEComponentState::ACTIVE && !this->active_logged_) {
    this->active_logged_ = true;
    ESP_LOGI(TAG, "BLE active");
  }
}

static const char *state_to_str(BLEComponentState state) {
  switch (state) {
    case BLEComponentState::OFF:
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

void RP2040BLE::packet_handler_(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
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
    default:
      break;
  }
}

}  // namespace esphome::rp2040_ble

#endif  // USE_RP2040_BLE
