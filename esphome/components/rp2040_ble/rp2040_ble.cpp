#include "rp2040_ble.h"

#ifdef USE_RP2040_BLE

#include <array>
#include <cstdint>
#include <type_traits>

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

template<typename T, size_t A, size_t B>
constexpr std::array<T, A + B> concat(const std::array<T, A> &a, const std::array<T, B> &b) {
  std::array<T, A + B> r{};
  for (size_t i = 0; i < A; ++i)
    r[i] = a[i];
  for (size_t j = 0; j < B; ++j)
    r[A + j] = b[j];
  return r;
}

constexpr size_t MAX_ADV_DATA_SIZE = 31;

constexpr std::array<uint8_t, 3> adv_flags = {2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06};

#ifdef USE_API_TRANSPORT_BLE
// clang-format off
constexpr std::array<uint8_t, 18> adv_uuid = {
  17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
  0x3e, 0x80, 0x39, 0x53, 0x54, 0x45, 0x64, 0x89, 0x44, 0x44, 0x9c, 0x1c, 0x8b, 0x0d, 0x1b, 0xe5
};
// clang-format on
#else
constexpr std::array<uint8_t, 0> adv_uuid = {};
#endif

template<size_t N> constexpr auto make_name_entry(const char (&name)[N]) {
  constexpr size_t max_name_size = MAX_ADV_DATA_SIZE - adv_flags.size() - adv_uuid.size() - 2;

  constexpr size_t name_size = N - 1;
  constexpr size_t size = name_size < max_name_size ? name_size : max_name_size;

  std::array<uint8_t, 2 + size> entry{};

  entry[0] = static_cast<uint8_t>(size + 1);
  entry[1] =
      name_size <= max_name_size ? BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME : BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME;

  for (size_t i = 0; i < size; ++i)
    entry[2 + i] = static_cast<uint8_t>(name[i]);

  return entry;
}

constexpr auto adv_data = concat(concat(adv_flags, adv_uuid), make_name_entry(BLE_DEVICE_NAME));

template<size_t N> constexpr auto build_profile_data(const char (&name)[N]) {
  constexpr std::size_t name_size = N - 1;  // exclude '\0'

  // ATT DB version
  constexpr std::array<uint8_t, 1> att_db = {1};

  // 0x0001 PRIMARY_SERVICE - GAP
  constexpr std::array<uint8_t, 10> primary_service = {
      0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,
  };

  // GAP Device Name characteristic
  // clang-format off
  std::array<uint8_t, 21 + name_size> device_name_char = {
    // Characteristic declaration (0x0002)
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a,
    // Characteristic value (0x0003)
    static_cast<uint8_t>(name_size + 8), 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x2a,
  };
  // clang-format on
  for (std::size_t i = 0; i < name_size; ++i)
    device_name_char[21 + i] = static_cast<uint8_t>(name[i]);

  // GAP Appearance characteristic
  // clang-format off
  constexpr std::array<uint8_t, 23> appearance_char = {
    // Characteristic declaration (0x0004)
    0x0d, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x28, 0x02, 0x05, 0x00, 0x01, 0x2a,
    // Characteristic value (0x0005): Appearance = 0x0003
    0x0a, 0x00, 0x02, 0x00, 0x05, 0x00, 0x01, 0x2a, 0x03, 0x00,
  };
  // clang-format on

  constexpr std::array<uint8_t, 2> end_markers = {0x00, 0x00};

  return concat(att_db, concat(primary_service, concat(device_name_char, concat(appearance_char, end_markers))));
}
constexpr auto profile_data = build_profile_data(BLE_DEVICE_NAME);

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
    att_server_init(profile_data.data(), NULL, NULL);

    this->btstack_initialized_ = true;

    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data.size(), (uint8_t *) adv_data.data());
    gap_advertisements_enable(1);
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
    case SM_EVENT_JUST_WORKS_REQUEST: {
      ESP_LOGI(TAG, "Just Works pairing requested, confirming automatically");
      sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::rp2040_ble

#endif  // USE_RP2040_BLE
