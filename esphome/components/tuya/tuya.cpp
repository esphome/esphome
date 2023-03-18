#include "tuya.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/gpio.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya";
static const int COMMAND_DELAY = 10;
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5;

void Tuya::setup() {
  this->set_interval("heartbeat", 15000, [this] { this->send_empty_command_(TuyaCommandType::HEARTBEAT); });
  if (this->status_pin_.has_value()) {
    this->status_pin_.value()->digital_write(false);
  }
}

void Tuya::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
  process_command_queue_();
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    if (this->init_failed_) {
      ESP_LOGCONFIG(TAG, "  Initialization failed. Current init_state: %u", static_cast<uint8_t>(this->init_state_));
    } else {
      ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                    static_cast<uint8_t>(this->init_state_));
    }
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Tuya device.");
    return;
  }
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDatapointType::RAW) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: raw (value: %s)", info.id, format_hex_pretty(info.value_raw).c_str());
    } else if (info.type == TuyaDatapointType::BOOLEAN) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: switch (value: %s)", info.id, ONOFF(info.value_bool));
    } else if (info.type == TuyaDatapointType::INTEGER) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: int value (value: %d)", info.id, info.value_int);
    } else if (info.type == TuyaDatapointType::STRING) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: string value (value: %s)", info.id, info.value_string.c_str());
    } else if (info.type == TuyaDatapointType::ENUM) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: enum (value: %d)", info.id, info.value_enum);
    } else if (info.type == TuyaDatapointType::BITMASK) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: bitmask (value: %x)", info.id, info.value_bitmask);
    } else {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: unknown", info.id);
    }
  }
  if ((this->status_pin_reported_ != -1) || (this->reset_pin_reported_ != -1)) {
    ESP_LOGCONFIG(TAG, "  GPIO Configuration: status: pin %d, reset: pin %d", this->status_pin_reported_,
                  this->reset_pin_reported_);
  }
  if (this->status_pin_.has_value()) {
    LOG_PIN("  Status Pin: ", this->status_pin_.value());
  }
  ESP_LOGCONFIG(TAG, "  Product: '%s'", this->product_.c_str());
}

bool Tuya::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0x55)
  if (at == 0)
    return new_byte == 0x55;
  // Byte 1: HEADER2 (always 0xAA)
  if (at == 1)
    return new_byte == 0xAA;

  // Byte 2: VERSION
  // no validation for the following fields:
  uint8_t version = data[2];
  if (at == 2)
    return true;
  // Byte 3: COMMAND
  uint8_t command = data[3];
  if (at == 3)
    return true;

  // Byte 4: LENGTH1
  // Byte 5: LENGTH2
  if (at <= 5) {
    // no validation for these fields
    return true;
  }

  uint16_t length = (uint16_t(data[4]) << 8) | (uint16_t(data[5]));

  // wait until all data is read
  if (at - 6 < length)
    return true;

  // Byte 6+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
  uint8_t rx_checksum = new_byte;
  uint8_t calc_checksum = 0;
  for (uint32_t i = 0; i < 6 + length; i++)
    calc_checksum += data[i];

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Tuya Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 6;
  ESP_LOGV(TAG, "Received Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", command, version,
           format_hex_pretty(message_data, length).c_str(), static_cast<uint8_t>(this->init_state_));
  this->handle_command_(command, version, message_data, length);

  // return false to reset rx buffer
  return false;
}

void Tuya::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  } else {
    this->last_rx_char_timestamp_ = millis();
  }
}

void Tuya::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  TuyaCommandType command_type = (TuyaCommandType) command;

  if (this->expected_response_.has_value() && this->expected_response_ == command_type) {
    this->expected_response_.reset();
    this->command_queue_.erase(command_queue_.begin());
    this->init_retries_ = 0;
  }

  switch (command_type) {
    case TuyaCommandType::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      this->protocol_version_ = version;
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
      }
      if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      }
      break;
    case TuyaCommandType::PRODUCT_QUERY: {
      // check it is a valid string made up of printable characters
      bool valid = true;
      for (size_t i = 0; i < len; i++) {
        if (!std::isprint(buffer[i])) {
          valid = false;
          break;
        }
      }
      if (valid) {
        this->product_ = std::string(reinterpret_cast<const char *>(buffer), len);
      } else {
        this->product_ = R"({"p":"INVALID"})";
      }
      if (this->init_state_ == TuyaInitState::INIT_PRODUCT) {
        this->init_state_ = TuyaInitState::INIT_CONF;
        this->send_empty_command_(TuyaCommandType::CONF_QUERY);
      }
      break;
    }
    case TuyaCommandType::CONF_QUERY: {
      if (len >= 2) {
        this->status_pin_reported_ = buffer[0];
        this->reset_pin_reported_ = buffer[1];
      }
      if (this->init_state_ == TuyaInitState::INIT_CONF) {
        // If mcu returned status gpio, then we can omit sending wifi state
        if (this->status_pin_reported_ != -1) {
          this->init_state_ = TuyaInitState::INIT_DATAPOINT;
          this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
          bool is_pin_equals =
              this->status_pin_.has_value() && this->status_pin_.value()->get_pin() == this->status_pin_reported_;
          // Configure status pin toggling (if reported and configured) or WIFI_STATE periodic send
          if (is_pin_equals) {
            ESP_LOGV(TAG, "Configured status pin %i", this->status_pin_reported_);
            this->set_interval("wifi", 1000, [this] { this->set_status_pin_(); });
          } else {
            ESP_LOGW(TAG, "Supplied status_pin does not equals the reported pin %i. TuyaMcu will work in limited mode.",
                     this->status_pin_reported_);
          }
        } else {
          this->init_state_ = TuyaInitState::INIT_WIFI;
          ESP_LOGV(TAG, "Configured WIFI_STATE periodic send");
          this->set_interval("wifi", 1000, [this] { this->send_wifi_status_(); });
        }
      }
      break;
    }
    case TuyaCommandType::WIFI_STATE:
      if (this->init_state_ == TuyaInitState::INIT_WIFI) {
        this->init_state_ = TuyaInitState::INIT_DATAPOINT;
        this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
      }
      break;
    case TuyaCommandType::WIFI_RESET:
      ESP_LOGE(TAG, "WIFI_RESET is not handled");
      break;
    case TuyaCommandType::WIFI_SELECT:
      ESP_LOGE(TAG, "WIFI_SELECT is not handled");
      break;
    case TuyaCommandType::DATAPOINT_DELIVER:
      break;
    case TuyaCommandType::DATAPOINT_REPORT:
      if (this->init_state_ == TuyaInitState::INIT_DATAPOINT) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->set_timeout("datapoint_dump", 1000, [this] { this->dump_config(); });
        this->initialized_callback_.call();
      }
      this->handle_datapoints_(buffer, len);
      break;
    case TuyaCommandType::DATAPOINT_QUERY:
      break;
    case TuyaCommandType::WIFI_TEST:
      this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_TEST, .payload = std::vector<uint8_t>{0x00, 0x00}});
      break;
    case TuyaCommandType::WIFI_RSSI:
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::WIFI_RSSI, .payload = std::vector<uint8_t>{get_wifi_rssi_()}});
      break;
    case TuyaCommandType::LOCAL_TIME_QUERY:
#ifdef USE_TIME
      if (this->time_id_.has_value()) {
        this->send_local_time_();
        auto *time_id = *this->time_id_;
        time_id->add_on_time_sync_callback([this] { this->send_local_time_(); });
      } else {
        ESP_LOGW(TAG, "LOCAL_TIME_QUERY is not handled because time is not configured");
      }
#else
      ESP_LOGE(TAG, "LOCAL_TIME_QUERY is not handled");
#endif
      break;
    case TuyaCommandType::VACUUM_MAP_UPLOAD:
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::VACUUM_MAP_UPLOAD, .payload = std::vector<uint8_t>{0x01}});
      ESP_LOGW(TAG, "Vacuum map upload requested, responding that it is not enabled.");
      break;
    case TuyaCommandType::GET_NETWORK_STATUS: {
      uint8_t wifi_status = this->get_wifi_status_code_();

      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::GET_NETWORK_STATUS, .payload = std::vector<uint8_t>{wifi_status}});
      ESP_LOGV(TAG, "Network status requested, reported as %i", wifi_status);
      break;
    }
    default:
      ESP_LOGE(TAG, "Invalid command (0x%02X) received", command);
  }
}

void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
  while (len >= 4) {
    TuyaDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDatapointType) buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = (buffer[2] << 8) + buffer[3];
    const uint8_t *data = buffer + 4;
    size_t data_len = len - 4;
    if (data_size > data_len) {
      ESP_LOGW(TAG, "Datapoint %u is truncated and cannot be parsed (%zu > %zu)", datapoint.id, data_size, data_len);
      return;
    }

    datapoint.len = data_size;

    switch (datapoint.type) {
      case TuyaDatapointType::RAW:
        datapoint.value_raw = std::vector<uint8_t>(data, data + data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, format_hex_pretty(datapoint.value_raw).c_str());
        break;
      case TuyaDatapointType::BOOLEAN:
        if (data_size != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad boolean len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_bool = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;
      case TuyaDatapointType::INTEGER:
        if (data_size != 4) {
          ESP_LOGW(TAG, "Datapoint %u has bad integer len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
        break;
      case TuyaDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
        break;
      case TuyaDatapointType::ENUM:
        if (data_size != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad enum len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_enum = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_enum);
        break;
      case TuyaDatapointType::BITMASK:
        switch (data_size) {
          case 1:
            datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
            break;
          case 2:
            datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
            break;
          case 4:
            datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
            break;
          default:
            ESP_LOGW(TAG, "Datapoint %u has bad bitmask len %zu", datapoint.id, data_size);
            return;
        }
        ESP_LOGD(TAG, "Datapoint %u update to %#08X", datapoint.id, datapoint.value_bitmask);
        break;
      default:
        ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
        return;
    }

    len -= data_size + 4;
    buffer = data + data_size;

    // drop update if datapoint is in ignore_mcu_datapoint_update list
    bool skip = false;
    for (auto i : this->ignore_mcu_update_on_datapoints_) {
      if (datapoint.id == i) {
        ESP_LOGV(TAG, "Datapoint %u found in ignore_mcu_update_on_datapoints list, dropping MCU update", datapoint.id);
        skip = true;
        break;
      }
    }
    if (skip)
      continue;

    // Update internal datapoints
    bool found = false;
    for (auto &other : this->datapoints_) {
      if (other.id == datapoint.id) {
        other = datapoint;
        found = true;
      }
    }
    if (!found) {
      this->datapoints_.push_back(datapoint);
    }

    // Run through listeners
    for (auto &listener : this->listeners_) {
      if (listener.datapoint_id == datapoint.id)
        listener.on_datapoint(datapoint);
    }
  }
}

void Tuya::send_raw_command_(TuyaCommand command) {
  uint8_t len_hi = (uint8_t)(command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t)(command.payload.size() & 0xFF);
  uint8_t version = 0;

  this->last_command_timestamp_ = millis();
  switch (command.cmd) {
    case TuyaCommandType::HEARTBEAT:
      this->expected_response_ = TuyaCommandType::HEARTBEAT;
      break;
    case TuyaCommandType::PRODUCT_QUERY:
      this->expected_response_ = TuyaCommandType::PRODUCT_QUERY;
      break;
    case TuyaCommandType::CONF_QUERY:
      this->expected_response_ = TuyaCommandType::CONF_QUERY;
      break;
    case TuyaCommandType::DATAPOINT_DELIVER:
    case TuyaCommandType::DATAPOINT_QUERY:
      this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT;
      break;
    default:
      break;
  }

  ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", static_cast<uint8_t>(command.cmd),
           version, format_hex_pretty(command.payload).c_str(), static_cast<uint8_t>(this->init_state_));

  this->write_array({0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
  if (!command.payload.empty())
    this->write_array(command.payload.data(), command.payload.size());

  uint8_t checksum = 0x55 + 0xAA + (uint8_t) command.cmd + len_hi + len_lo;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);
}

void Tuya::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) {
    this->rx_message_.clear();
  }

  if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
    this->expected_response_.reset();
    if (init_state_ != TuyaInitState::INIT_DONE) {
      if (++this->init_retries_ >= MAX_RETRIES) {
        this->init_failed_ = true;
        ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
        this->command_queue_.erase(command_queue_.begin());
        this->init_retries_ = 0;
      }
    } else {
      this->command_queue_.erase(command_queue_.begin());
    }
  }

  // Left check of delay since last command in case there's ever a command sent by calling send_raw_command_ directly
  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty() &&
      !this->expected_response_.has_value()) {
    this->send_raw_command_(command_queue_.front());
    if (!this->expected_response_.has_value())
      this->command_queue_.erase(command_queue_.begin());
  }
}

void Tuya::send_command_(const TuyaCommand &command) {
  command_queue_.push_back(command);
  process_command_queue_();
}

void Tuya::send_empty_command_(TuyaCommandType command) {
  send_command_(TuyaCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
}

void Tuya::set_status_pin_() {
  bool is_network_ready = network::is_connected() && remote_is_connected();
  this->status_pin_.value()->digital_write(is_network_ready);
}

uint8_t Tuya::get_wifi_status_code_() {
  uint8_t status = 0x02;

  if (network::is_connected()) {
    status = 0x03;

    // Protocol version 3 also supports specifying when connected to "the cloud"
    if (this->protocol_version_ >= 0x03 && remote_is_connected()) {
      status = 0x04;
    }
  } else {
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
      status = 0x01;
    }
#endif
  };

  return status;
}

uint8_t Tuya::get_wifi_rssi_() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->wifi_rssi();
#endif

  return 0;
}

void Tuya::send_wifi_status_() {
  uint8_t status = this->get_wifi_status_code_();

  if (status == this->wifi_status_) {
    return;
  }

  ESP_LOGD(TAG, "Sending WiFi Status");
  this->wifi_status_ = status;
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_STATE, .payload = std::vector<uint8_t>{status}});
}

#ifdef USE_TIME
void Tuya::send_local_time_() {
  std::vector<uint8_t> payload;
  auto *time_id = *this->time_id_;
  time::ESPTime now = time_id->now();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    // Tuya days starts from Monday, esphome uses Sunday as day 1
    uint8_t day_of_week = now.day_of_week - 1;
    if (day_of_week == 0) {
      day_of_week = 7;
    }
    ESP_LOGD(TAG, "Sending local time");
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second, day_of_week};
  } else {
    // By spec we need to notify MCU that the time was not obtained if this is a response to a query
    ESP_LOGW(TAG, "Sending missing local time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::LOCAL_TIME_QUERY, .payload = payload});
}
#endif

void Tuya::set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, false);
}

void Tuya::set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BOOLEAN, value, 1, false);
}

void Tuya::set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, false);
}

void Tuya::set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, false);
}

void Tuya::set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, false);
}

void Tuya::set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BITMASK, value, length, false);
}

void Tuya::force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, true);
}

void Tuya::force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BOOLEAN, value, 1, true);
}

void Tuya::force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, true);
}

void Tuya::force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, true);
}

void Tuya::force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, true);
}

void Tuya::force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BITMASK, value, length, true);
}

optional<TuyaDatapoint> Tuya::get_datapoint_(uint8_t datapoint_id) {
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      return datapoint;
  }
  return {};
}

void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
                                        uint8_t length, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %u", datapoint_id, value);
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != datapoint_type) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_uint == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }

  std::vector<uint8_t> data;
  switch (length) {
    case 4:
      data.push_back(value >> 24);
      data.push_back(value >> 16);
    case 2:
      data.push_back(value >> 8);
    case 1:
      data.push_back(value >> 0);
      break;
    default:
      ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
      return;
  }
  this->send_datapoint_command_(datapoint_id, datapoint_type, data);
}

void Tuya::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, format_hex_pretty(value).c_str());
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != TuyaDatapointType::RAW) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_raw == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::RAW, value);
}

void Tuya::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, value.c_str());
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != TuyaDatapointType::STRING) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_string == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  std::vector<uint8_t> data;
  for (char const &c : value) {
    data.push_back(c);
  }
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::STRING, data);
}

void Tuya::send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data) {
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());

  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = buffer});
}

void Tuya::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      func(datapoint);
  }
}

TuyaInitState Tuya::get_init_state() { return this->init_state_; }

}  // namespace tuya
}  // namespace esphome
