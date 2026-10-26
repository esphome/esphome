#include "delta_wallbox.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef USE_ESP32

namespace esphome::delta_wallbox {
ESPHOME_LOG_TAG(TAG, "delta_wallbox");
// Modbus function codes
static constexpr uint8_t FC_READ_HOLDING_REGISTERS = 0x03;
static constexpr uint8_t FC_WRITE_SINGLE_REGISTER = 0x06;
static constexpr uint8_t FC_WRITE_MULTIPLE_REGISTERS = 0x10;
static constexpr uint8_t FC_EXCEPTION_FLAG = 0x80;

// Response lengths in bytes, CRC included
static constexpr size_t READ_RESPONSE_OVERHEAD = 5;
static constexpr size_t WRITE_RESPONSE_LEN = 8;
static constexpr size_t EXCEPTION_RESPONSE_LEN = 5;

// Frame bodies without CRC
static constexpr size_t LOGIN_BODY_LEN = 35;
static constexpr size_t LOGIN_PASS_LEN = 22;
static constexpr size_t LOGIN_USER_LEN = 6;
static constexpr size_t TIME_SYNC_BODY_LEN = 15;

static constexpr uint8_t REGISTER_BYTE_COUNT = 2;
static constexpr uint8_t TWO_REGISTER_BYTE_COUNT = 4;
static constexpr uint8_t CHARGING_INFO_BYTE_COUNT = 18;
static constexpr uint8_t SERIAL_TEXT_BYTE_COUNT = 14;
static constexpr uint32_t RETRANSMIT_TIMEOUT_MS = 4000;
static constexpr uint32_t RETRANSMIT_TIMEOUT_ID = 0;
static constexpr uint8_t MAX_PAIRING_ATTEMPTS = 5;
static constexpr uint16_t PAIRING_STATUS_READY = 0x0007;
static constexpr uint16_t PAIRING_STATUS_WAITING = 0x0008;
static constexpr uint8_t LOGIN_ACCEPTED = 0x01;

static constexpr uint8_t CMD_POLL_CHARGING[] = {0x01, 0x03, 0x01, 0x1B, 0x00, 0x09};
static constexpr uint8_t CMD_GET_MODEL[] = {0x01, 0x03, 0x01, 0x85, 0x00, 0x0E};
static constexpr uint8_t CMD_GET_BT_FW_VERSION[] = {0x02, 0x03, 0x01, 0x00, 0x00, 0x01};
static constexpr uint8_t CMD_CHECK_PAIRING[] = {0x02, 0x03, 0x01, 0x00, 0x00, 0x03};
static constexpr uint8_t CMD_GET_LOGIN_USER[] = {0x01, 0x03, 0x01, 0x7D, 0x00, 0x03};
static constexpr uint8_t CMD_READ_LOGIN_STATUS[] = {0x01, 0x03, 0x01, 0x80, 0x00, 0x01};
static constexpr uint8_t CMD_GET_LOGIN_ROLE[] = {0x01, 0x03, 0x01, 0x81, 0x00, 0x01};
static constexpr uint8_t CMD_GET_CHARGER_FW_VERSION[] = {0x01, 0x03, 0x01, 0x10, 0x00, 0x02};
static constexpr uint8_t CMD_GET_MAX_CURRENT[] = {0x01, 0x03, 0x01, 0x12, 0x00, 0x02};
static constexpr uint8_t CMD_GET_SERIAL_NUMBER[] = {0x01, 0x03, 0x01, 0x09, 0x00, 0x01};
static constexpr uint8_t CMD_GET_ERROR_CODE[] = {0x01, 0x03, 0x01, 0xB5, 0x00, 0x01};
static constexpr uint8_t CMD_START_CHARGING[] = {0x01, 0x06, 0x01, 0x02, 0x00, 0x01};
static constexpr uint8_t CMD_STOP_CHARGING[] = {0x01, 0x06, 0x01, 0x03, 0x00, 0x01};

static constexpr uint16_t REG_MAX_CURRENT = 0x0112;

// Indexed by DeltaState
PROGMEM_STRING_TABLE(DeltaStateStrings, "CHECK_PAIRING", "CONNECTED", "GET_CHARGER_FW_VERSION", "GET_BT_FW_VERSION",
                     "GET_LOGIN_RESULT", "GET_LOGIN_RESULT0", "GET_LOGIN_ROLE", "GET_LOGIN_USER", "GET_MODEL",
                     "GET_SERIAL_NUMBER", "HALTED", "IDLE", "SEND_LOGIN_CREDENTIALS", "TIME_SYNC");
static_assert(DeltaStateStrings::LAST_INDEX == static_cast<uint8_t>(DeltaState::DELTA_STATE_TIME_SYNC),
              "DeltaStateStrings must list every DeltaState");
static const LogString *delta_state_to_string(DeltaState state) {
  return DeltaStateStrings::get_log_str(static_cast<uint8_t>(state), DeltaStateStrings::LAST_INDEX);
}

// The function code a handshake step waits for; 0 where any frame is handled
static uint8_t expected_function_code(DeltaState state) {
  switch (state) {
    case DeltaState::DELTA_STATE_IDLE:
    case DeltaState::DELTA_STATE_HALTED:
    case DeltaState::DELTA_STATE_CONNECTED:
      return 0;
    case DeltaState::DELTA_STATE_SEND_LOGIN_CREDENTIALS:
    case DeltaState::DELTA_STATE_TIME_SYNC:
      return FC_WRITE_MULTIPLE_REGISTERS;
    default:
      return FC_READ_HOLDING_REGISTERS;
  }
}

// UUID references
static const esp32_ble_tracker::ESPBTUUID SERVICE_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static const esp32_ble_tracker::ESPBTUUID TX_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
static const esp32_ble_tracker::ESPBTUUID RX_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

// Appends the little-endian Modbus CRC-16 to the len bytes in frame and returns the new length.
static size_t append_crc(uint8_t *frame, size_t len) {
  const uint16_t crc = crc16(frame, static_cast<uint16_t>(len));
  frame[len] = static_cast<uint8_t>(crc & 0xFF);
  frame[len + 1] = static_cast<uint8_t>(crc >> 8);
  return len + 2;
}

static bool verify_crc(const uint8_t *data, size_t len) {
  if (len < EXCEPTION_RESPONSE_LEN)
    return false;
  const uint16_t calculated = crc16(data, static_cast<uint16_t>(len - 2));
  const uint16_t received = data[len - 2] | (data[len - 1] << 8);
  return calculated == received;
}

// Login write body: 22-byte password and 6-byte user, both left-padded with zeros
static std::array<uint8_t, LOGIN_BODY_LEN> build_login_body(StringRef user, StringRef password) {
  std::array<uint8_t, LOGIN_BODY_LEN> frame = {
      0x01, 0x10, 0x01, 0x72, 0x00, 0x0E, 0x1C,  // Unit, FC16, register 0x0172, 14 registers, 28 bytes
  };

  const size_t pass_len = std::min(password.size(), LOGIN_PASS_LEN);
  std::copy_n(password.c_str(), pass_len, frame.begin() + 7 + (LOGIN_PASS_LEN - pass_len));

  uint8_t *user_chunk = frame.data() + 7 + LOGIN_PASS_LEN;
  bool is_digit = !user.empty();
  for (size_t i = 0; i < user.size() && is_digit; i++) {
    is_digit = isdigit(static_cast<unsigned char>(user.c_str()[i]));
  }
  if (is_digit) {
    const uint64_t val = strtoull(user.c_str(), nullptr, 10);
    for (size_t i = 0; i < LOGIN_USER_LEN; i++) {
      user_chunk[LOGIN_USER_LEN - 1 - i] = static_cast<uint8_t>((val >> (8 * i)) & 0xFF);
    }
  } else {
    const size_t user_len = std::min(user.size(), LOGIN_USER_LEN);
    std::copy_n(user.c_str(), user_len, user_chunk + LOGIN_USER_LEN - user_len);
  }
  return frame;
}

static std::array<uint8_t, TIME_SYNC_BODY_LEN> build_time_sync_body(int year, int month, int day, int hour, int minute,
                                                                    int second, int weekday) {
  const uint8_t weekday_bitmap = static_cast<uint8_t>(1 << weekday);  // Sunday=bit0, Mon=bit1...
  return {
      0x01,
      0x10,
      0x01,
      0x14,
      0x00,
      0x04,
      0x08,  // Unit, FC16, register 0x0114, 4 registers, 8 bytes
      static_cast<uint8_t>(year % 100),
      static_cast<uint8_t>(month),
      weekday_bitmap,
      static_cast<uint8_t>(day),
      static_cast<uint8_t>(hour),
      static_cast<uint8_t>(minute),
      static_cast<uint8_t>(second),
      0x00,
  };
}

void DeltaWallbox::dump_config() {
  ESP_LOGCONFIG(TAG, "Delta Wallbox:");
  ESP_LOGCONFIG(TAG, "  User: %s", this->user_.c_str());
  ESP_LOGCONFIG(TAG, "  BLE PIN configured: %s", this->has_pin_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  LOG_UPDATE_INTERVAL(this);
}

void DeltaWallbox::update() {
  if (this->is_connected()) {
    // Start poll flow sequence
    ESP_LOGD(TAG, "Polling charging info...");
    this->send_frame_(CMD_POLL_CHARGING, sizeof(CMD_POLL_CHARGING));
  }
}

void DeltaWallbox::write_frame_(const uint8_t *frame, size_t len) {
  if (this->tx_char_handle_ == 0) {
    ESP_LOGW(TAG, "TX characteristic handle is not ready while in state %s. Dropping %u-byte frame.",
             LOG_STR_ARG(delta_state_to_string(this->state_)), (unsigned int) len);
    return;
  }
  ESP_LOGD(TAG, "Sending %u-byte frame in state %s (conn_id=%u, tx_handle=0x%04X).", (unsigned int) len,
           LOG_STR_ARG(delta_state_to_string(this->state_)), this->parent_->get_conn_id(), this->tx_char_handle_);
  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->tx_char_handle_, len,
                               const_cast<uint8_t *>(frame), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
  }
}

// Copies body into a stack buffer, appends the CRC and writes the frame.
void DeltaWallbox::send_frame_(const uint8_t *body, size_t len) {
  std::array<uint8_t, MAX_FRAME_LEN> frame{};
  std::copy_n(body, len, frame.begin());
  this->write_frame_(frame.data(), append_crc(frame.data(), len));
}

void DeltaWallbox::drop_rx_bytes_(size_t count) {
  count = std::min(count, this->rx_len_);
  std::memmove(this->rx_buffer_.data(), this->rx_buffer_.data() + count, this->rx_len_ - count);
  this->rx_len_ -= count;
}

void DeltaWallbox::send_next_state_command_() {
  ESP_LOGD(TAG, "Preparing command for state %s.", LOG_STR_ARG(delta_state_to_string(this->state_)));
  switch (this->state_) {
    case DeltaState::DELTA_STATE_GET_MODEL:
      ESP_LOGI(TAG, "Requesting charger model name...");
      this->send_frame_(CMD_GET_MODEL, sizeof(CMD_GET_MODEL));
      break;
    case DeltaState::DELTA_STATE_GET_BT_FW_VERSION:
      ESP_LOGI(TAG, "Requesting BT firmware version...");
      this->send_frame_(CMD_GET_BT_FW_VERSION, sizeof(CMD_GET_BT_FW_VERSION));
      break;
    case DeltaState::DELTA_STATE_CHECK_PAIRING:
      ESP_LOGI(TAG, "Checking pairing status...");
      this->send_frame_(CMD_CHECK_PAIRING, sizeof(CMD_CHECK_PAIRING));
      break;
    case DeltaState::DELTA_STATE_GET_LOGIN_USER:
      ESP_LOGI(TAG, "Querying user profile...");
      this->send_frame_(CMD_GET_LOGIN_USER, sizeof(CMD_GET_LOGIN_USER));
      break;
    case DeltaState::DELTA_STATE_GET_LOGIN_RESULT0:
      ESP_LOGI(TAG, "Checking pre-login status register...");
      this->send_frame_(CMD_READ_LOGIN_STATUS, sizeof(CMD_READ_LOGIN_STATUS));
      break;
    case DeltaState::DELTA_STATE_SEND_LOGIN_CREDENTIALS: {
      ESP_LOGI(TAG, "Transmitting login credentials...");
      const auto body = build_login_body(this->user_, this->password_);
      this->send_frame_(body.data(), body.size());
      break;
    }
    case DeltaState::DELTA_STATE_GET_LOGIN_RESULT:
      ESP_LOGI(TAG, "Verifying login result...");
      this->send_frame_(CMD_READ_LOGIN_STATUS, sizeof(CMD_READ_LOGIN_STATUS));
      break;
    case DeltaState::DELTA_STATE_GET_LOGIN_ROLE:
      ESP_LOGI(TAG, "Checking role privileges...");
      this->send_frame_(CMD_GET_LOGIN_ROLE, sizeof(CMD_GET_LOGIN_ROLE));
      break;
    case DeltaState::DELTA_STATE_TIME_SYNC: {
#ifdef USE_TIME
      // Only a valid time is written; the charger keeps its own clock otherwise
      if (this->time_id_ != nullptr) {
        auto tm = this->time_id_->now();
        if (tm.is_valid()) {
          ESP_LOGI(TAG, "Synchronizing wallbox clock...");
          // ESPTime day_of_week is 1 (Sunday) to 7 (Saturday); the charger counts from 0
          const auto body = build_time_sync_body(tm.year, tm.month, tm.day_of_month, tm.hour, tm.minute, tm.second,
                                                 tm.day_of_week - 1);
          this->send_frame_(body.data(), body.size());
          break;
        }
      }
#endif
      this->advance_to_(DeltaState::DELTA_STATE_GET_CHARGER_FW_VERSION);
      break;
    }
    case DeltaState::DELTA_STATE_GET_CHARGER_FW_VERSION:
      ESP_LOGI(TAG, "Requesting charger main firmware version...");
      this->send_frame_(CMD_GET_CHARGER_FW_VERSION, sizeof(CMD_GET_CHARGER_FW_VERSION));
      break;
    case DeltaState::DELTA_STATE_GET_SERIAL_NUMBER:
      ESP_LOGI(TAG, "Requesting wallbox serial number...");
      this->send_frame_(CMD_GET_SERIAL_NUMBER, sizeof(CMD_GET_SERIAL_NUMBER));
      break;
    default:
      this->cancel_timeout(RETRANSMIT_TIMEOUT_ID);
      return;
  }
  this->set_timeout(RETRANSMIT_TIMEOUT_ID, RETRANSMIT_TIMEOUT_MS, [this]() { this->retransmit_(); });
}

void DeltaWallbox::retransmit_() {
  ESP_LOGW(TAG, "No answer in login state %s within %u ms. Sending the command again...",
           LOG_STR_ARG(delta_state_to_string(this->state_)), (unsigned int) RETRANSMIT_TIMEOUT_MS);
  this->send_next_state_command_();
}

void DeltaWallbox::advance_to_(DeltaState next) {
  ESP_LOGD(TAG, "State transition: %s -> %s", LOG_STR_ARG(delta_state_to_string(this->state_)),
           LOG_STR_ARG(delta_state_to_string(next)));
  this->state_ = next;
  this->send_next_state_command_();
}

void DeltaWallbox::handle_frame_(const uint8_t *frame, size_t len) {
  ESP_LOGD(TAG, "Received valid frame in state %s: func=0x%02X len=%u",
           LOG_STR_ARG(delta_state_to_string(this->state_)), frame[1], (unsigned int) len);
  if (frame[1] & FC_EXCEPTION_FLAG) {
    ESP_LOGE(TAG, "Modbus exception received in state %s! Error code: 0x%02X",
             LOG_STR_ARG(delta_state_to_string(this->state_)), frame[2]);
    if (this->state_ == DeltaState::DELTA_STATE_TIME_SYNC) {
      // The clock write is optional, so a charger that refuses it is used without it
      ESP_LOGW(TAG, "Charger refused the clock sync; continuing without it");
      this->advance_to_(DeltaState::DELTA_STATE_GET_CHARGER_FW_VERSION);
    } else if (expected_function_code(this->state_) != 0) {
      // The charger refused a handshake step; sending the same request again would not change that
      this->halt_(LOG_STR("Modbus exception during login"));
    }
    return;
  }
  const uint8_t expected = expected_function_code(this->state_);
  if (expected != 0 && frame[1] != expected) {
    // A late answer to a retransmitted request; the pending one is answered or sent again by the timer
    ESP_LOGD(TAG, "Ignoring func=0x%02X reply while waiting for 0x%02X", frame[1], expected);
    return;
  }

  switch (this->state_) {
    case DeltaState::DELTA_STATE_IDLE:
      break;  // No command is pending; ignore the frame.
    case DeltaState::DELTA_STATE_GET_MODEL: {
      char model[MAX_FRAME_LEN];
      const size_t data_len = std::min<size_t>(frame[2], len - READ_RESPONSE_OVERHEAD);
      size_t model_len = 0;
      for (size_t i = 0; i < data_len; i++) {
        if (frame[3 + i] != '\0')
          model[model_len++] = static_cast<char>(frame[3 + i]);
      }
      model[model_len] = '\0';
      ESP_LOGI(TAG, "Discovered Wallbox Model Name: %s", model);
      if (this->model_name_text_sensor_ != nullptr) {
        this->model_name_text_sensor_->publish_state(model);
      }
      this->advance_to_(DeltaState::DELTA_STATE_GET_BT_FW_VERSION);
      break;
    }
    case DeltaState::DELTA_STATE_GET_BT_FW_VERSION: {
      if (frame[2] >= REGISTER_BYTE_COUNT) {
        uint8_t major = frame[3] % 16;
        uint8_t minor = frame[4];
        char ver[16];
        snprintf(ver, sizeof(ver), "%d.%d", major, minor);
        ESP_LOGI(TAG, "Discovered Wallbox BT FW: %s", ver);
        if (this->bt_fw_version_text_sensor_ != nullptr) {
          this->bt_fw_version_text_sensor_->publish_state(ver);
        }
      }
      this->advance_to_(DeltaState::DELTA_STATE_CHECK_PAIRING);
      break;
    }
    case DeltaState::DELTA_STATE_CHECK_PAIRING: {
      if (frame[2] >= REGISTER_BYTE_COUNT) {
        uint16_t status = (frame[3] << 8) | frame[4];
        ESP_LOGI(TAG, "Pairing Status Register: 0x%04X", status);
        if (status == PAIRING_STATUS_READY) {
          this->pairing_in_progress_ = false;
          this->pairing_attempts_ = 0;
          ESP_LOGI(TAG, "Pairing ready and confirmed!");
          this->advance_to_(DeltaState::DELTA_STATE_GET_LOGIN_USER);
        } else if (status == PAIRING_STATUS_WAITING) {
          ESP_LOGW(TAG, "Wallbox is waiting to be paired by the system controller (attempt %u, in_progress=%s).",
                   this->pairing_attempts_ + 1,
                   this->pairing_in_progress_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
          this->pairing_attempts_++;
          if (!this->pairing_in_progress_) {
            ESP_LOGI(TAG, "Initiating BLE pairing with wallbox.");
            auto err = this->parent_->pair();
            if (err != ESP_OK) {
              ESP_LOGW(TAG, "Failed to initiate BLE pairing, err=%d", err);
            } else {
              this->pairing_in_progress_ = true;
            }
          }
        } else {
          ESP_LOGW(TAG, "Unexpected pairing status 0x%04X", status);
          this->pairing_attempts_++;
        }
        if (this->pairing_attempts_ >= MAX_PAIRING_ATTEMPTS) {
          this->halt_(LOG_STR("Too many failed/waiting pairing checks"));
        }
      } else {
        ESP_LOGW(TAG, "Pairing status response too short: byte_count=%u frame_len=%u", frame[2], (unsigned int) len);
      }
      break;
    }
    case DeltaState::DELTA_STATE_GET_LOGIN_USER: {
      this->advance_to_(DeltaState::DELTA_STATE_GET_LOGIN_RESULT0);
      break;
    }
    case DeltaState::DELTA_STATE_GET_LOGIN_RESULT0: {
      if (frame[2] == REGISTER_BYTE_COUNT) {
        this->advance_to_(DeltaState::DELTA_STATE_SEND_LOGIN_CREDENTIALS);
      }
      break;
    }
    case DeltaState::DELTA_STATE_SEND_LOGIN_CREDENTIALS: {
      this->advance_to_(DeltaState::DELTA_STATE_GET_LOGIN_RESULT);
      break;
    }
    case DeltaState::DELTA_STATE_GET_LOGIN_RESULT: {
      // Only the one-register status answer counts; a late reply to another read has a different length
      if (frame[2] == REGISTER_BYTE_COUNT) {
        uint8_t login_val = frame[4];  // respondent data_lo
        if (login_val == LOGIN_ACCEPTED) {
          ESP_LOGI(TAG, "Credentials accepted! Logged in as %s.", this->user_.c_str());
          this->advance_to_(DeltaState::DELTA_STATE_GET_LOGIN_ROLE);
        } else {
          ESP_LOGE(TAG, "Login validation failed (0x%02X). Check password.", login_val);
          this->halt_(LOG_STR("Bad credentials"));
        }
      }
      break;
    }
    case DeltaState::DELTA_STATE_HALTED: {
      break;  // Authentication is halted; ignore any late replies.
    }
    case DeltaState::DELTA_STATE_GET_LOGIN_ROLE: {
      if (frame[2] == REGISTER_BYTE_COUNT) {
        this->advance_to_(DeltaState::DELTA_STATE_TIME_SYNC);
      }
      break;
    }
    case DeltaState::DELTA_STATE_TIME_SYNC: {
      this->advance_to_(DeltaState::DELTA_STATE_GET_CHARGER_FW_VERSION);
      break;
    }
    case DeltaState::DELTA_STATE_GET_CHARGER_FW_VERSION: {
      if (frame[2] >= TWO_REGISTER_BYTE_COUNT) {
        char ver[16];
        snprintf(ver, sizeof(ver), "%d.%d", frame[3], frame[4]);
        ESP_LOGI(TAG, "Discovered Charger Main FW: %s", ver);
        if (this->fw_version_text_sensor_ != nullptr) {
          this->fw_version_text_sensor_->publish_state(ver);
        }
      }
      this->advance_to_(DeltaState::DELTA_STATE_GET_SERIAL_NUMBER);
      break;
    }
    case DeltaState::DELTA_STATE_GET_SERIAL_NUMBER: {
      // Serial read (0x0109) returns 14 ASCII bytes instead of one register
      if (frame[2] >= SERIAL_TEXT_BYTE_COUNT) {
        char serial[SERIAL_TEXT_BYTE_COUNT + 1];
        size_t serial_len = 0;
        for (size_t i = 0; i < SERIAL_TEXT_BYTE_COUNT; i++) {
          if (frame[3 + i] != '\0')
            serial[serial_len++] = static_cast<char>(frame[3 + i]);
        }
        serial[serial_len] = '\0';
        ESP_LOGI(TAG, "Wallbox serial text: %s", serial);
        if (this->serial_number_text_sensor_ != nullptr)
          this->serial_number_text_sensor_->publish_state(serial);
      }
      this->advance_to_(DeltaState::DELTA_STATE_CONNECTED);
      ESP_LOGI(TAG, "Authentication complete. Core parameters loaded.");
      this->status_clear_warning();
      this->update();  // perform first poll update immediately
      break;
    }
    case DeltaState::DELTA_STATE_CONNECTED: {
      // Connected polling responses
      if (frame[1] == FC_READ_HOLDING_REGISTERS) {
        uint8_t byte_count = frame[2];
        if (byte_count == CHARGING_INFO_BYTE_COUNT) {
          // Parsing 0x011B charging info (9 registers)
          const ChargingInfo info = decode_charging_info(frame);
          if (this->power_sensor_ != nullptr)
            this->power_sensor_->publish_state(info.power_kw);
          if (this->energy_sensor_ != nullptr)
            this->energy_sensor_->publish_state(info.energy_kwh);
          if (this->charging_time_sensor_ != nullptr)
            this->charging_time_sensor_->publish_state(info.charging_minutes);
          if (this->charger_state_sensor_ != nullptr)
            this->charger_state_sensor_->publish_state(info.state);

          // Retrieve maximum current output setting next
          this->send_frame_(CMD_GET_MAX_CURRENT, sizeof(CMD_GET_MAX_CURRENT));
        } else if (byte_count == TWO_REGISTER_BYTE_COUNT) {
          // Parsing 0x0112 max output setting (2 registers)
          // frame[4] is the configured max (set by DIP switch in device)
          // frame[5] the active limit
          this->max_current_limit_ = frame[4];
          if (this->max_current_sensor_ != nullptr)
            this->max_current_sensor_->publish_state(frame[5]);
          if (this->max_current_number_ != nullptr)
            this->max_current_number_->publish_state(frame[5]);

          // Poll the error code register every cycle
          this->send_frame_(CMD_GET_ERROR_CODE, sizeof(CMD_GET_ERROR_CODE));
        } else if (byte_count == REGISTER_BYTE_COUNT) {
          // Error status register (0x01B5)
          uint16_t reg_val = (frame[3] << 8) | frame[4];
          char hex_str[16];
          snprintf(hex_str, sizeof(hex_str), "%04X", reg_val);
          if (this->error_code_text_sensor_ != nullptr)
            this->error_code_text_sensor_->publish_state(hex_str);
        }
      } else if (frame[1] == FC_WRITE_SINGLE_REGISTER) {
        const uint16_t register_addr = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
        if (register_addr == REG_MAX_CURRENT) {
          ESP_LOGI(TAG, "Wallbox max current command acknowledged: %u A", frame[5]);
          if (this->max_current_number_ != nullptr)
            this->max_current_number_->publish_state(frame[5]);
        } else {
          // Confirmation frame of Start/Stop write actions
          ESP_LOGI(TAG, "Wallbox charging command successfully acknowledged.");
        }
      }
      break;
    }
  }
}

void DeltaWallbox::reset_state_() {
  ESP_LOGW(TAG, "Resetting component state from %s. pairing_attempts=%u, pairing_in_progress=%s, rx_buffer=%u bytes",
           LOG_STR_ARG(delta_state_to_string(this->state_)), this->pairing_attempts_,
           this->pairing_in_progress_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"), (unsigned int) this->rx_len_);
  this->cancel_timeout(RETRANSMIT_TIMEOUT_ID);
  this->rx_len_ = 0;
  this->state_ = DeltaState::DELTA_STATE_IDLE;
  this->max_current_limit_ = 0;  // refuse writes until the next 0x0112 read
  this->pairing_attempts_ = 0;
  this->pairing_in_progress_ = false;
  this->status_set_warning();
}

void DeltaWallbox::halt_(const LogString *reason) {
  ESP_LOGE(TAG, "Halting authentication: %s. A reconnect is required to retry.", LOG_STR_ARG(reason));
  this->cancel_timeout(RETRANSMIT_TIMEOUT_ID);
  this->rx_len_ = 0;
  this->state_ = DeltaState::DELTA_STATE_HALTED;
  this->pairing_attempts_ = 0;
  this->pairing_in_progress_ = false;
  this->status_set_warning(reason);
}

void DeltaWallbox::set_charging_state(bool state) {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot send charging command while wallbox is not fully connected.");
    return;
  }

  if (state) {
    ESP_LOGI(TAG, "Sending remote START charging command...");
    this->send_frame_(CMD_START_CHARGING, sizeof(CMD_START_CHARGING));
  } else {
    ESP_LOGI(TAG, "Sending remote STOP charging command...");
    this->send_frame_(CMD_STOP_CHARGING, sizeof(CMD_STOP_CHARGING));
  }
}

void DeltaWallbox::set_max_current(float current) {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot set maximum current while wallbox is not fully connected.");
    return;
  }
  const uint8_t upper = this->max_current_limit_;
  if (upper == 0) {
    ESP_LOGW(TAG, "Wallbox current limit not read yet; refusing to set %.0f A", current);
    return;
  }
  const int32_t rounded = std::lround(current);
  if (rounded < 1) {
    ESP_LOGW(TAG, "Rejecting maximum current %d A", static_cast<int>(rounded));
    return;
  }
  const uint8_t requested = static_cast<uint8_t>(std::min<int32_t>(rounded, upper));
  if (rounded > upper) {
    ESP_LOGW(TAG, "Requested %d A exceeds wallbox limit %u A; using %u A", static_cast<int>(rounded), upper, upper);
  }
  ESP_LOGI(TAG, "Setting wallbox maximum current to %u A", requested);
  const std::array<uint8_t, 6> body = {0x01,
                                       FC_WRITE_SINGLE_REGISTER,
                                       static_cast<uint8_t>(REG_MAX_CURRENT >> 8),
                                       static_cast<uint8_t>(REG_MAX_CURRENT & 0xFF),
                                       0x00,
                                       requested};
  this->send_frame_(body.data(), body.size());
}

void DeltaWallbox::start_charging() { this->set_charging_state(true); }

void DeltaWallbox::stop_charging() { this->set_charging_state(false); }

void DeltaWallbox::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected from Bluetooth wallbox client (conn_id=%u, reason=%d). Resetting state.",
               param->disconnect.conn_id, param->disconnect.reason);
      this->reset_state_();
      this->node_state = esp32_ble_tracker::ClientState::IDLE;
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      ESP_LOGI(TAG, "GATT open event received: status=%d conn_id=%u", param->open.status, param->open.conn_id);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGI(TAG, "GATT service discovery completed: status=%d", param->search_cmpl.status);
      auto *tx_char = this->parent_->get_characteristic(SERVICE_UUID, TX_UUID);
      auto *rx_char = this->parent_->get_characteristic(SERVICE_UUID, RX_UUID);
      if (tx_char == nullptr || rx_char == nullptr) {
        ESP_LOGE(TAG, "Could not discover matching NUS TX/RX service on this BLE device.");
        this->reset_state_();
        break;
      }
      this->tx_char_handle_ = tx_char->handle;
      this->rx_char_handle_ = rx_char->handle;

      esp_err_t err = this->parent_->register_for_notify(this->rx_char_handle_);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed, status=%d", err);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGI(TAG, "Register-for-notify event: status=%d handle=0x%04X", param->reg_for_notify.status,
               param->reg_for_notify.handle);
      if (param->reg_for_notify.handle == this->rx_char_handle_) {
        ESP_LOGI(TAG, "Subscribed to RX notification handle 0x%04X successfully.", this->rx_char_handle_);
        this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
        if (this->state_ == DeltaState::DELTA_STATE_IDLE) {
          ESP_LOGI(TAG, "GATT connection established. Starting authentication state machine.");
          this->advance_to_(DeltaState::DELTA_STATE_GET_MODEL);
        }
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->rx_char_handle_)
        break;

      const size_t value_len = param->notify.value_len;
      ESP_LOGD(TAG, "Notification received on handle 0x%04X with %u bytes. Buffer before append=%u bytes.",
               param->notify.handle, param->notify.value_len, (unsigned int) this->rx_len_);
      if (value_len > MAX_FRAME_LEN - this->rx_len_) {
        ESP_LOGW(TAG, "Notification overflows the %u-byte RX buffer. Discarding buffered data.",
                 (unsigned int) MAX_FRAME_LEN);
        this->rx_len_ = 0;
        if (value_len > MAX_FRAME_LEN)
          break;
      }
      std::memcpy(this->rx_buffer_.data() + this->rx_len_, param->notify.value, value_len);
      this->rx_len_ += value_len;

      bool progress = true;
      while (progress && this->rx_len_ >= 3) {
        progress = false;
        uint8_t func_code = this->rx_buffer_[1];
        size_t expected_len = 0;

        if (func_code == FC_READ_HOLDING_REGISTERS) {
          expected_len = this->rx_buffer_[2] + READ_RESPONSE_OVERHEAD;
        } else if (func_code == FC_WRITE_MULTIPLE_REGISTERS || func_code == FC_WRITE_SINGLE_REGISTER) {
          expected_len = WRITE_RESPONSE_LEN;
        } else if (func_code == (FC_EXCEPTION_FLAG | FC_READ_HOLDING_REGISTERS) ||
                   func_code == (FC_EXCEPTION_FLAG | FC_WRITE_MULTIPLE_REGISTERS) ||
                   func_code == (FC_EXCEPTION_FLAG | FC_WRITE_SINGLE_REGISTER)) {
          expected_len = EXCEPTION_RESPONSE_LEN;
        }

        if (expected_len > 0) {
          if (this->rx_len_ >= expected_len) {
            if (verify_crc(this->rx_buffer_.data(), expected_len)) {
              // The frame stays in rx_buffer_ during handle_frame_(); it is dropped afterwards.
              this->handle_frame_(this->rx_buffer_.data(), expected_len);
              this->drop_rx_bytes_(expected_len);
              progress = true;
            } else {
              ESP_LOGW(TAG, "CRC mismatch while parsing notification data in state %s. Dropping one byte from buffer.",
                       LOG_STR_ARG(delta_state_to_string(this->state_)));
              // Shift by one byte to realign with the next possible frame start
              this->drop_rx_bytes_(1);
              progress = true;
            }
          }
        } else {
          ESP_LOGW(TAG,
                   "Unsupported function code 0x%02X while parsing notification data in state %s. Advancing buffer.",
                   func_code, LOG_STR_ARG(delta_state_to_string(this->state_)));
          this->drop_rx_bytes_(1);
          progress = true;
        }
      }
      break;
    }
    default:
      break;
  }
}

void DeltaWallbox::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
      if (!this->parent_->check_addr(param->ble_security.ble_req.bd_addr))
        return;

      if (!this->has_pin_) {
        ESP_LOGW(TAG, "Wallbox requested a BLE passkey, but no pin is configured. Set pin: under delta_wallbox:.");
        return;
      }

      ESP_LOGI(TAG, "Wallbox requested a BLE passkey. Replying with configured YAML pin.");
      esp_err_t err = esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, this->pin_);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send BLE passkey reply: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (!this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;

      this->pairing_in_progress_ = false;
      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGI(TAG,
                 "BLE pairing completed successfully. auth_mode=%d addr_type=%d. Re-checking wallbox pairing status.",
                 param->ble_security.auth_cmpl.auth_mode, param->ble_security.auth_cmpl.addr_type);
        this->pairing_attempts_ = 0;
        if (this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED &&
            this->state_ == DeltaState::DELTA_STATE_CHECK_PAIRING) {
          this->advance_to_(DeltaState::DELTA_STATE_CHECK_PAIRING);
        }
      } else {
        ESP_LOGW(TAG, "BLE pairing failed, reason=%d", param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    default:
      break;
  }
}

}  // namespace esphome::delta_wallbox

#endif
