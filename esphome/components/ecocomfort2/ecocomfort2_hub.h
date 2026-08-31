#pragma once
#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "ecocomfort2_child.h"

#include <vector>

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

#include <esp_gattc_api.h>

namespace esphome {
namespace ecocomfort2 {

namespace espbt = esphome::esp32_ble_tracker;

// Forward declare
class Ecocomfort2Client;

// BLE UUIDs
static const espbt::ESPBTUUID ECOCOMFORT2_SERVICE_UUID =
    espbt::ESPBTUUID::from_raw("f4b827c3-e660-4bc8-bdf6-3c8e9b845e0d");
static const espbt::ESPBTUUID ECOCOMFORT2_C_INFO_UUID =
    espbt::ESPBTUUID::from_raw("f5f56229-dd4f-480f-a829-9189269d8b37");
static const espbt::ESPBTUUID ECOCOMFORT2_C_STATE_UUID =
    espbt::ESPBTUUID::from_raw("438d3433-7e5a-459a-a8e4-66343fad2bb0");
static const espbt::ESPBTUUID ECOCOMFORT2_C_OPER_UUID =
    espbt::ESPBTUUID::from_raw("b9d6f678-bc0d-4a73-90c8-60b0f07301f1");
static const espbt::ESPBTUUID ECOCOMFORT2_C_CONFIG_UUID =
    espbt::ESPBTUUID::from_raw("d3dac48e-b4e1-4f3a-8715-326ddf1da89a");
static const espbt::ESPBTUUID ECOCOMFORT2_C_CLOCK_UUID =
    espbt::ESPBTUUID::from_raw("82788997-49e4-4533-b949-7ed433678044");
static const espbt::ESPBTUUID ECOCOMFORT2_C_ADVANCED_UUID =
    espbt::ESPBTUUID::from_raw("f8b2284e-61dd-44e3-a782-a93c9503ab2d");

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

// Configuration write special values
static const uint8_t CONFIG_PRESERVE = 0x7F;

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
  void loop() override;
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
  bool has_state_data() const { return this->state_valid_; }
  bool has_oper_data() const { return this->oper_valid_; }
  bool has_config_data() const { return this->config_valid_; }
  bool has_advanced_data() const { return this->advanced_valid_; }

  // --- Write commands (called by child entities) ---

  /** Send operating mode command (preset + speed). Debounced by 100ms. */
  void send_operation_command(uint8_t preset, uint8_t speed, bool auto_mode);

  /** Write season to C_CONFIGURATION. */
  void write_season(bool is_summer);

  /** Write free cooling level (0-3) to C_CONFIGURATION. */
  void write_free_cooling(uint8_t level);

  /** Write all three thresholds to C_CONFIGURATION.
   *  Threshold values should be plain (0-3). Advanced flags are read from internal state.
   */
  void write_thresholds(uint8_t humidity, uint8_t luminosity, uint8_t voc);

  /** Set humidity advanced flag (used by switch before calling write_thresholds). */
  void set_humidity_advanced(bool advanced) { this->humidity_advanced_ = advanced; }
  /** Set VOC advanced flag (used by switch before calling write_thresholds). */
  void set_voc_advanced(bool advanced) { this->voc_advanced_ = advanced; }

  /** Write calibration offsets to C_ADVANCED. */
  void write_offsets(int16_t temp_offset_raw, int16_t humidity_offset_raw);

  /** Send BLE pairing request. */
  void pair();

  /** Send clock sync. */
  bool send_clock();

#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif

  // --- Current state accessors (for child entities) ---

  // State from C_STATE readback
  float get_temperature() const { return this->temperature_; }
  float get_humidity() const { return this->humidity_; }
  uint16_t get_voc() const { return this->voc_; }
  uint8_t get_direction() const { return this->direction_; }

  // State from C_SETTING_OPER readback
  uint8_t get_actual_mode() const { return this->actual_mode_; }
  uint8_t get_actual_speed() const { return this->actual_speed_; }
  bool get_boost_active() const { return this->boost_active_; }
  bool get_night_active() const { return this->night_active_; }
  bool get_auto_active() const { return this->auto_active_; }

  // State from C_CONFIGURATION readback
  uint8_t get_role() const { return this->role_; }
  uint8_t get_humidity_threshold() const { return this->humidity_threshold_; }
  uint8_t get_luminosity_threshold() const { return this->luminosity_threshold_; }
  uint8_t get_voc_threshold() const { return this->voc_threshold_; }
  bool get_humidity_advanced() const { return this->humidity_advanced_; }
  bool get_voc_advanced() const { return this->voc_advanced_; }
  bool get_season_summer() const { return this->season_summer_; }
  uint8_t get_free_cooling_level() const { return this->free_cooling_level_; }

  // State from C_ADVANCED readback
  int16_t get_temp_offset_raw() const { return this->temp_offset_raw_; }
  int16_t get_humidity_offset_raw() const { return this->humidity_offset_raw_; }

  // Firmware version from C_INFO
  static constexpr uint8_t FIRMWARE_VERSION_SIZE = 16;
  const char *get_firmware_version() const { return this->firmware_version_; }

  // Desired state (persisted)
  uint8_t get_desired_preset() const { return this->persisted_.preset; }
  uint8_t get_desired_speed() const { return this->persisted_.speed; }
  bool get_desired_auto_mode() const { return this->persisted_.auto_mode; }

  /** Skip-send guard: set to true during readback to prevent feedback loops. */
  bool skip_send_{false};

 protected:
  std::vector<Ecocomfort2Client *> children_;
  void dispatch_status_();
  void dispatch_config_();
  void dispatch_connect_(bool connected);

  bool discover_characteristics_();

  // BLE characteristic handles
  uint16_t char_handle_info_{0};
  uint16_t char_handle_state_{0};
  uint16_t char_handle_oper_{0};
  uint16_t char_handle_config_{0};
  uint16_t char_handle_clock_{0};
  uint16_t char_handle_advanced_{0};

  // Write helpers
  bool write_characteristic_(uint16_t handle, const uint8_t *data, uint16_t len);
  void do_send_operation_command_();
  void request_encryption_(const char *reason);
  const char *log_id_() const;

  void on_encryption_ready_();

  // Parsed state from readbacks
  // C_STATE
  float temperature_{0};
  float humidity_{0};
  uint16_t voc_{0};
  uint8_t direction_{0};

  // C_SETTING_OPER
  uint8_t actual_mode_{0};
  uint8_t actual_speed_{0};
  bool boost_active_{false};
  bool night_active_{false};
  bool auto_active_{false};

  // C_CONFIGURATION
  uint8_t role_{0};
  uint8_t humidity_threshold_{0};
  uint8_t luminosity_threshold_{0};
  uint8_t voc_threshold_{0};
  bool humidity_advanced_{false};
  bool voc_advanced_{false};
  bool season_summer_{false};
  uint8_t free_cooling_level_{0};

  // C_ADVANCED
  int16_t temp_offset_raw_{0};
  int16_t humidity_offset_raw_{0};

  // C_INFO
  char firmware_version_[FIRMWARE_VERSION_SIZE]{};
  bool firmware_read_{false};

  // Persisted desired state
  Ecocomfort2PersistedState persisted_{OPER_IN_OUT, SPEED_VEL1, false};
  ESPPreferenceObject pref_;

  // Debounce
  uint8_t pending_preset_{0};
  uint8_t pending_speed_{0};
  bool pending_auto_mode_{false};

  // Clock sync tracking
  uint32_t last_clock_sync_{0};
  uint32_t last_clock_attempt_{0};
  static const uint32_t CLOCK_SYNC_INTERVAL = 3600000;      // 1 hour in ms
  static const uint32_t CLOCK_SYNC_RETRY_INTERVAL = 60000;  // 1 minute in ms
  static const uint32_t ENCRYPTION_TIMEOUT_MS = 10000;
  static const uint8_t MAX_ENCRYPTION_RETRIES = 3;

  // Connection state tracking
  bool ready_{false};  // true once BLE authentication/encryption is confirmed
  uint8_t encryption_retries_{0};
  bool state_valid_{false};
  bool oper_valid_{false};
  bool config_valid_{false};
  bool advanced_valid_{false};

#ifdef USE_TIME
  time::RealTimeClock *time_id_{nullptr};
#endif

  // Parse readback data
  bool parse_state_(const uint8_t *data, uint16_t len);
  bool parse_oper_(const uint8_t *data, uint16_t len);
  bool parse_config_(const uint8_t *data, uint16_t len);
  bool parse_advanced_(const uint8_t *data, uint16_t len);
  bool parse_info_(const uint8_t *data, uint16_t len);
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif
