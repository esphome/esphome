#pragma once
#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "ecocomfort2_child.h"

#include <vector>

#include <esp_gattc_api.h>

namespace esphome::ecocomfort2 {

namespace espbt = esphome::esp32_ble_tracker;

// Forward declare
class Ecocomfort2Client;

// BLE UUIDs
static const espbt::ESPBTUUID ECOCOMFORT2_SERVICE_UUID =
    espbt::ESPBTUUID::from_raw("f4b827c3-e660-4bc8-bdf6-3c8e9b845e0d");
static const espbt::ESPBTUUID ECOCOMFORT2_C_OPER_UUID =
    espbt::ESPBTUUID::from_raw("b9d6f678-bc0d-4a73-90c8-60b0f07301f1");

// Operating mode byte values
static const uint8_t OPER_OFF = 0x00;
static const uint8_t OPER_IN = 0x01;
static const uint8_t OPER_OUT = 0x02;
static const uint8_t OPER_IN_OUT = 0x03;
static const uint8_t OPER_SENSOR_OR_AUTO = 0x04;

// Speed constants
static const uint8_t SPEED_OFF = 0x00;
static const uint8_t SPEED_SLEEP = 0x01;
static const uint8_t SPEED_VEL1 = 0x02;
static const uint8_t SPEED_VEL2 = 0x03;
static const uint8_t SPEED_VEL3 = 0x04;

// Speed flags
static const uint8_t FLAG_AUTO = 0x10;
static const uint8_t FLAG_BOOST = 0x40;
static const uint8_t FLAG_NIGHT = 0x80;

// Preset names
static const char *const PRESET_IN = "In";
static const char *const PRESET_OUT = "Out";
static const char *const PRESET_IN_OUT = "In-Out";
static const char *const PRESET_SENSOR = "Sensor";
static const char *const PRESET_AUTO = "Auto";

// Persisted state structure
struct Ecocomfort2PersistedState {
  uint8_t preset;  // OPER_* value
  uint8_t speed;   // SPEED_* value for the last non-auto command
  bool auto_mode;  // true when the desired preset is Auto
};

/** Hub component for Ecocomfort 2.0 VMC via BLE. */
class Ecocomfort2Hub : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  /** Register a child component. */
  void register_child(Ecocomfort2Client *obj);

  /** @return true if BLE connection is established. */
  bool is_connected() const { return this->node_state == espbt::ClientState::ESTABLISHED; }
  /** @return true if BLE connection and encryption are both established. */
  bool is_ready() const { return this->is_connected() && this->ready_; }
  bool has_oper_data() const { return this->oper_valid_; }

  // --- Write commands (called by child entities) ---

  /** Send operating mode command (preset + speed). Debounced by 100ms. */
  void send_operation_command(uint8_t preset, uint8_t speed, bool auto_mode);

  // --- Current state accessors (for child entities) ---

  // State from C_SETTING_OPER readback
  uint8_t get_actual_mode() const { return this->actual_mode_; }
  uint8_t get_actual_speed() const { return this->actual_speed_; }
  bool get_boost_active() const { return this->boost_active_; }
  bool get_night_active() const { return this->night_active_; }
  bool get_auto_active() const { return this->auto_active_; }

  // Desired state (persisted)
  uint8_t get_desired_preset() const { return this->persisted_.preset; }
  uint8_t get_desired_speed() const { return this->persisted_.speed; }
  bool get_desired_auto_mode() const { return this->persisted_.auto_mode; }

  /** Skip-send guard: set to true during readback to prevent feedback loops. */
  bool skip_send_{false};

 protected:
  std::vector<Ecocomfort2Client *> children_;
  void dispatch_status_();
  void dispatch_connect_(bool connected);

  bool discover_characteristics_();

  // BLE characteristic handles
  uint16_t char_handle_oper_{0};

  // Write helpers
  bool write_characteristic_(uint16_t handle, const uint8_t *data, uint16_t len);
  void do_send_operation_command_();
  void request_encryption_(const char *reason);
  const char *log_id_() const;

  void on_encryption_ready_();

  // Parsed state from readbacks
  // C_SETTING_OPER
  uint8_t actual_mode_{0};
  uint8_t actual_speed_{0};
  bool boost_active_{false};
  bool night_active_{false};
  bool auto_active_{false};

  // Persisted desired state
  Ecocomfort2PersistedState persisted_{OPER_IN_OUT, SPEED_VEL1, false};
  ESPPreferenceObject pref_;

  // Debounce
  uint8_t pending_preset_{0};
  uint8_t pending_speed_{0};
  bool pending_auto_mode_{false};

  static const uint32_t ENCRYPTION_TIMEOUT_MS = 10000;
  static const uint8_t MAX_ENCRYPTION_RETRIES = 3;

  // Connection state tracking
  bool ready_{false};  // true once BLE authentication/encryption is confirmed
  uint8_t encryption_retries_{0};
  bool oper_valid_{false};

  // Parse readback data
  bool parse_oper_(const uint8_t *data, uint16_t len);
};

}  // namespace esphome::ecocomfort2

#endif
