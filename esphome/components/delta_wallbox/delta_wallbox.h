#pragma once

#ifdef USE_ESP32

#include <array>
#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome::delta_wallbox {
// Largest FC03 response: unit, function, byte count, 255 data bytes, CRC.
static constexpr size_t MAX_FRAME_LEN = 260;
enum class DeltaState {
  DELTA_STATE_IDLE,
  DELTA_STATE_GET_MODEL,
  DELTA_STATE_GET_BT_FW_VERSION,
  DELTA_STATE_CHECK_PAIRING,
  DELTA_STATE_GET_LOGIN_USER,
  DELTA_STATE_GET_LOGIN_RESULT0,
  DELTA_STATE_SEND_LOGIN_CREDENTIALS,
  DELTA_STATE_GET_LOGIN_RESULT,
  DELTA_STATE_GET_LOGIN_ROLE,
  DELTA_STATE_TIME_SYNC,
  DELTA_STATE_GET_CHARGER_FW_VERSION,
  DELTA_STATE_CONNECTED,
};

class DeltaWallbox : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void dump_config() override;

  void set_user(const char *user) { this->user_ = StringRef(user); }
  void set_password(const char *password) { this->password_ = StringRef(password); }
  void set_pin(uint32_t pin) {
    pin_ = pin;
    has_pin_ = true;
  }
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { time_id_ = time_id; }
#endif

  // Sensors
  void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
  void set_energy_sensor(sensor::Sensor *s) { energy_sensor_ = s; }
  void set_charging_time_sensor(sensor::Sensor *s) { charging_time_sensor_ = s; }
  void set_charger_state_sensor(sensor::Sensor *s) { charger_state_sensor_ = s; }
  void set_max_current_sensor(sensor::Sensor *s) { max_current_sensor_ = s; }
  // Text Sensors
  void set_serial_number_sensor(text_sensor::TextSensor *s) { serial_number_sensor_ = s; }
  void set_fw_version_sensor(text_sensor::TextSensor *s) { fw_version_sensor_ = s; }
  void set_bt_fw_version_sensor(text_sensor::TextSensor *s) { bt_fw_version_sensor_ = s; }
  void set_model_name_sensor(text_sensor::TextSensor *s) { model_name_sensor_ = s; }
  void set_error_code_sensor(text_sensor::TextSensor *s) { error_code_sensor_ = s; }
  // Switch Interface
  void set_charging_state(bool state);
  void start_charging();
  void stop_charging();
  bool is_charging();

  bool is_connected() {
    return this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED &&
           this->state_ == DeltaState::DELTA_STATE_CONNECTED;
  }

 protected:
  void write_frame_(const uint8_t *frame, size_t len);
  void send_frame_(const uint8_t *body, size_t len);
  void send_next_state_command_();
  void advance_to_(DeltaState next);
  void handle_frame_(const uint8_t *frame, size_t len);
  void drop_rx_bytes_(size_t count);
  void reset_state_();

  StringRef user_;
  StringRef password_;
  uint32_t pin_{0};
  bool has_pin_{false};
#ifdef USE_TIME
  time::RealTimeClock *time_id_{nullptr};
#endif

  uint16_t rx_char_handle_{0};
  uint16_t tx_char_handle_{0};
  DeltaState state_{DeltaState::DELTA_STATE_IDLE};
  std::array<uint8_t, MAX_FRAME_LEN> rx_buffer_{};
  size_t rx_len_{0};
  uint32_t last_tx_time_{0};
  uint8_t pairing_attempts_{0};
  bool pairing_in_progress_{false};
  bool pending_charging_state_{false};
  bool has_pending_charging_action_{false};
  bool is_charging_{false};

  // Sensors
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *charging_time_sensor_{nullptr};
  sensor::Sensor *charger_state_sensor_{nullptr};
  sensor::Sensor *max_current_sensor_{nullptr};

  // Text Sensors
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  text_sensor::TextSensor *fw_version_sensor_{nullptr};
  text_sensor::TextSensor *bt_fw_version_sensor_{nullptr};
  text_sensor::TextSensor *model_name_sensor_{nullptr};
  text_sensor::TextSensor *error_code_sensor_{nullptr};
};
}  // namespace esphome::delta_wallbox
#endif
