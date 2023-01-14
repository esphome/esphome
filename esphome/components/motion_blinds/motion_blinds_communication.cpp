#ifdef USE_ESP32

#include "motion_blinds_communication.h"
#include <sstream>
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esphome/components/homeassistant/time/homeassistant_time.h"
#include "crypto.h"
#include <sys/time.h>

namespace esphome {
namespace motion_blinds {

static const char *const TAG = "motionblinds_communication";

static const int WANTED_MTU = 512;
static const char *const COMMAND_USER_QUERY = "02C005";
static const char *const COMMAND_SET_USER_KEY = "02C001";
static const char *const COMMAND_SET_TIME = "09A001";
static const char *const NOTIFY_MESSAGE_PHONE_USER = "0cc0060505";

static void hex_string_to_bytes(const std::string &hex_string, MotionBlindsMessage &message);
static std::string bytes_to_hex(const uint8_t *bytes, size_t length);
static uint8_t char_to_byte(char byte_char);
static std::string format_week_day(int day);
static std::string make_raw_command(const std::string &command);
static void append_time_string(std::stringstream &buffer);
static void append_set_time_string(std::stringstream &buffer);
static std::string decode_value(uint8_t *data, size_t length);
time::ESPTime get_time();

MotionBlindsCommunication::MotionBlindsCommunication() : write_char_handle_(0) {}

void MotionBlindsCommunication::connect() {
  if (this->parent_->connected())
    return;

  this->parent_->connect();
}

void MotionBlindsCommunication::disconnect() {
  this->parent_->disconnect();
  this->node_state = espbt::ClientState::IDLE;
}

void MotionBlindsCommunication::send_command(const std::string &command) {
  auto raw_command = make_raw_command(command);
  auto encrypted = Crypto::encrypt(raw_command);

  hex_string_to_bytes(encrypted, message_);

  message_.raw_command = command;

  esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->write_char_handle_,
                           message_.length, message_.bytes, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
}

void MotionBlindsCommunication::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                                    esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "Disconnected: %d", param->disconnect.reason);
      this->has_mtu_change_ = false;
      this->node_state = espbt::ClientState::IDLE;
      this->write_char_handle_ = 0;
      this->on_disconnected();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *notify_chara =
          this->parent_->get_characteristic(MOTION_BLINDS_SERVICE_UUID, MOTION_BLINDS_NOTIFY_CHARACTERISTIC_UUID);

      if (notify_chara != nullptr) {
        this->notify_char_handle_ = notify_chara->handle;
        auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                        notify_chara->handle);
        if (status) {
          ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d",
                   this->get_logging_device_name().c_str(), status);
        }

        status =
            esp_ble_gattc_write_char_descr(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                           notify_chara->get_descriptor(MOTION_BLINDS_NOTIFY_DESCRIPTOR)->handle, 2,
                                           (uint8_t *) "\x01\x00", ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);

        if (status) {
          ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char_descr failed, status=%d",
                   this->get_logging_device_name().c_str(), status);
        }
      } else {
        ESP_LOGW(TAG, "[%s] Could not find notification characteristic", this->get_logging_device_name().c_str());
      }

      auto *write_chara =
          this->parent_->get_characteristic(MOTION_BLINDS_SERVICE_UUID, MOTION_BINDS_WRITE_CHARACTERISTIC_UUID);
      if (write_chara != nullptr) {
        this->write_char_handle_ = write_chara->handle;
        if ((write_chara->properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY) != 0) {
          esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                            write_chara->handle);
        }
      } else {
        ESP_LOGW(TAG, "[%s] Could not find write characteristic", this->get_logging_device_name().c_str());
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (this->has_mtu_change_) {
        send_command(COMMAND_USER_QUERY);
      } else {
        esp_ble_gatt_set_local_mtu(WANTED_MTU);
        esp_ble_gattc_send_mtu_req(this->parent_->get_gattc_if(), this->parent_->get_conn_id());
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_char_handle_)
        break;

      auto value = decode_value(param->notify.value, param->notify.value_len);
      if (value.find(NOTIFY_MESSAGE_PHONE_USER) == 0) {
        send_command(COMMAND_SET_USER_KEY);
      } else {
        this->on_notify(value);
      }
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.mtu == WANTED_MTU) {
        this->has_mtu_change_ = true;
        if (this->write_char_handle_ != 0) {
          send_command(COMMAND_USER_QUERY);
        }
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (this->message_.raw_command == COMMAND_SET_USER_KEY) {
        send_set_time_();
      } else if (this->message_.raw_command.find(COMMAND_SET_TIME) == 0) {
        this->node_state = espbt::ClientState::ESTABLISHED;
      }
      break;
    }
    default:
      break;
  }
}

void MotionBlindsCommunication::send_set_time_() {
  std::stringstream buffer;
  buffer << COMMAND_SET_TIME;
  append_set_time_string(buffer);
  send_command(buffer.str());
}

std::string MotionBlindsCommunication::format_hex_num(size_t value, bool prefix) {
  std::stringstream stream;
  stream << std::hex << value;
  auto str1 = stream.str();
  auto length = str1.length();
  stream.clear();
  stream.str("");

  std::string str2 = str1;
  if ((length == 1 && !prefix) || length == 3) {
    stream.clear();
    stream << '0' << str1;
    str2 = stream.str();
  }

  if (length != 1) {
    if (length == 2) {
      if (prefix) {
        stream.clear();
        stream << "00" << str1;
        str2 = stream.str();
      }
    }
  } else if (prefix) {
    stream.clear();
    stream << "000" << str1;
    str2 = stream.str();
  }
  return str2;
}

std::string make_raw_command(const std::string &command) {
  std::stringstream buffer;
  buffer << command;
  append_time_string(buffer);
  return buffer.str();
}

void append_time_string(std::stringstream &buffer) {
  const auto local = get_time();
  struct timeval time_val = {};
  gettimeofday(&time_val, nullptr);

  // Last 2 chars of year
  auto year = MotionBlindsCommunication::format_hex_num(local.year % 100, false);
  auto month = MotionBlindsCommunication::format_hex_num(local.month, false);
  auto day = MotionBlindsCommunication::format_hex_num(local.day_of_month, false);
  auto hour = MotionBlindsCommunication::format_hex_num(local.hour, false);
  auto minute = MotionBlindsCommunication::format_hex_num(local.minute, false);
  auto seconds = MotionBlindsCommunication::format_hex_num(local.second, false);
  auto milliseconds = MotionBlindsCommunication::format_hex_num(time_val.tv_usec / 1000, true);

  buffer << year << month << day << hour << minute << seconds << milliseconds;
}

void hex_string_to_bytes(const std::string &hex_string, MotionBlindsMessage &message) {
  message.length = hex_string.length() / 2;
  for (size_t i = 0; i < hex_string.length(); i += 2) {
    message.bytes[i / 2] = char_to_byte(hex_string[i + 1]) | (char_to_byte(hex_string[i]) << 4);
  }
}

std::string bytes_to_hex(const uint8_t *bytes, size_t length) {
  std::stringstream buffer;
  static const char *hex_chars = "0123456789abcdef";

  for (size_t i = 0; i < length; ++i) {
    buffer << hex_chars[bytes[i] >> 4 & 0xF];
    buffer << hex_chars[bytes[i] & 0xF];
  }
  return buffer.str();
}

uint8_t char_to_byte(char byte_char) {
  if (byte_char >= '0' && byte_char <= '9') {
    return byte_char - '0';
  } else if (byte_char >= 'a' && byte_char <= 'f') {
    return byte_char - 'a' + 10;
  } else if (byte_char >= 'A' && byte_char <= 'F') {
    return byte_char - 'A' + 10;
  } else {
    return 0;
  }
}

/*
 * For some strange reason, set time is in a different format...
 */
void append_set_time_string(std::stringstream &buffer) {
  auto local = get_time();

  // Last 2 chars of year
  auto year = MotionBlindsCommunication::format_hex_num(local.year % 100, false);
  auto week_day = format_week_day(local.day_of_week);
  auto hour = MotionBlindsCommunication::format_hex_num(local.hour, false);
  auto minute = MotionBlindsCommunication::format_hex_num(local.minute, false);
  auto seconds = MotionBlindsCommunication::format_hex_num(local.second, false);
  auto month = MotionBlindsCommunication::format_hex_num(local.month, false);
  auto day = MotionBlindsCommunication::format_hex_num(local.day_of_month, false);

  buffer << week_day << hour << minute << seconds << year << month << day;
}

std::string format_week_day(int day) { return MotionBlindsCommunication::format_hex_num(day - 1, false); }

std::string decode_value(uint8_t *data, size_t length) { return Crypto::decrypt(bytes_to_hex(data, length)); }

time::ESPTime get_time() {
  auto *home_assistant_time = esphome::homeassistant::global_homeassistant_time;

  if (home_assistant_time != nullptr)
    return home_assistant_time->now();

  ESP_LOGW(TAG, "Home Assistant time not available, using local time");

  return time::ESPTime::from_epoch_local(::time(nullptr));
}

}  // namespace motion_blinds
}  // namespace esphome

#endif  // USE_ESP32
