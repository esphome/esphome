#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::delta_wallbox {

inline uint64_t read_be(const uint8_t *p, size_t n) {
  uint64_t value = 0;
  for (size_t i = 0; i < n; i++)
    value = (value << 8) | p[i];
  return value;
}

struct ChargingInfo {
  float power_kw;
  float energy_kwh;
  float charging_minutes;
  uint8_t state;
};

// Decodes the 0x011B charging info response, starting at the unit byte: [3..9] session energy (Wh),
// [10..13] power (W), [14..16] hours, minutes and seconds, [17] state, [18..20] unused.
inline ChargingInfo decode_charging_info(const uint8_t *frame) {
  const uint32_t power_raw = std::min<uint32_t>(encode_value<uint32_t>(&frame[10]), 99999);
  const uint64_t energy_raw = std::min<uint64_t>(read_be(&frame[3], 7), 999900);
  const uint8_t hours = std::min<uint8_t>(frame[14], 99);
  const uint8_t minutes = std::min<uint8_t>(frame[15], 59);
  const uint8_t seconds = std::min<uint8_t>(frame[16], 59);
  return {power_raw / 1000.0f, energy_raw / 1000.0f, hours * 60.0f + minutes + seconds / 60.0f, frame[17]};
}

}  // namespace esphome::delta_wallbox

#ifdef USE_ESP32

#include <array>
#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/string_ref.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome::delta_wallbox {
// Largest FC03 response: unit, function, byte count, 255 data bytes, CRC.
static constexpr size_t MAX_FRAME_LEN = 260;
enum class DeltaState : uint8_t {
  DELTA_STATE_CHECK_PAIRING,
  DELTA_STATE_CONNECTED,
  DELTA_STATE_GET_CHARGER_FW_VERSION,
  DELTA_STATE_GET_BT_FW_VERSION,
  DELTA_STATE_GET_LOGIN_RESULT,
  DELTA_STATE_GET_LOGIN_RESULT0,
  DELTA_STATE_GET_LOGIN_ROLE,
  DELTA_STATE_GET_LOGIN_USER,
  DELTA_STATE_GET_MODEL,
  DELTA_STATE_GET_SERIAL_NUMBER,
  DELTA_STATE_HALTED,
  DELTA_STATE_IDLE,
  DELTA_STATE_SEND_LOGIN_CREDENTIALS,
  DELTA_STATE_TIME_SYNC,
};

class DeltaWallbox : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  DeltaWallbox(const char *user, const char *password) : user_(user), password_(password) {}
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void dump_config() override;

  void set_pin(uint32_t pin) {
    this->pin_ = pin;
    this->has_pin_ = true;
  }
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif

  SUB_SENSOR(power)
  SUB_SENSOR(energy)
  SUB_SENSOR(charging_time)
  SUB_SENSOR(charger_state)
  SUB_SENSOR(max_current)
  SUB_NUMBER(max_current)  // settable max-current number entity
  SUB_TEXT_SENSOR(serial_number)
  SUB_TEXT_SENSOR(fw_version)
  SUB_TEXT_SENSOR(bt_fw_version)
  SUB_TEXT_SENSOR(model_name)
  SUB_TEXT_SENSOR(error_code)
  void set_charging_state(bool state);
  void set_max_current(float current);
  void start_charging();
  void stop_charging();

  bool is_connected() {
    return this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED &&
           this->state_ == DeltaState::DELTA_STATE_CONNECTED;
  }

 protected:
  void write_frame_(const uint8_t *frame, size_t len);
  void send_frame_(const uint8_t *body, size_t len);
  void send_next_state_command_();
  void retransmit_();
  void advance_to_(DeltaState next);
  void handle_frame_(const uint8_t *frame, size_t len);
  void drop_rx_bytes_(size_t count);
  void reset_state_();
  // Stops the handshake until the next BLE disconnect; used for failures a retry can't fix.
  void halt_(const LogString *reason);

  // Members ordered by size so nothing is padded
  StringRef user_;
  StringRef password_;
  uint32_t pin_{0};
#ifdef USE_TIME
  time::RealTimeClock *time_id_{nullptr};
#endif
  size_t rx_len_{0};

  uint16_t rx_char_handle_{0};
  uint16_t tx_char_handle_{0};
  DeltaState state_{DeltaState::DELTA_STATE_IDLE};
  uint8_t pairing_attempts_{0};
  uint8_t max_current_limit_{0};
  bool has_pin_{false};
  bool pairing_in_progress_{false};
  std::array<uint8_t, MAX_FRAME_LEN> rx_buffer_{};
};
}  // namespace esphome::delta_wallbox
#endif
