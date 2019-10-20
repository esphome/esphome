#include "tuya.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya";

void Tuya::setup() {
  this->send_empty_command_(TuyaCommandType::MCU_CONF);
  this->set_interval("heartbeat", 1000, [this] { this->send_empty_command_(TuyaCommandType::HEARTBEAT); });
}

void Tuya::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if ((gpio_status_ != -1) || (gpio_reset_ != -1))
    ESP_LOGCONFIG(TAG, "  GPIO MCU configuration not supported!");
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDatapointType::BOOLEAN)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: switch (value: %s)", info.id, ONOFF(info.value_bool));
    else if (info.type == TuyaDatapointType::INTEGER)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: int value (value: %d)", info.id, info.value_int);
    else if (info.type == TuyaDatapointType::ENUM)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: enum (value: %d)", info.id, info.value_enum);
    else if (info.type == TuyaDatapointType::BITMASK)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: bitmask (value: %x)", info.id, info.value_bitmask);
    else
      ESP_LOGCONFIG(TAG, "  Datapoint %d: unknown", info.id);
  }
  this->check_uart_settings(9600);
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
  if (at <= 5)
    // no validation for these fields
    return true;

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
  ESP_LOGV(TAG, "Received Tuya: CMD=0x%02X VERSION=%u DATA=[%s]", command, version,
           hexencode(message_data, length).c_str());
  this->handle_command_(command, version, message_data, length);

  // return false to reset rx buffer
  return false;
}

void Tuya::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  }
}

void Tuya::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  uint8_t c;
  switch ((TuyaCommandType) command) {
    case TuyaCommandType::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->send_empty_command_(TuyaCommandType::QUERY_STATE);
      }
      break;
    case TuyaCommandType::QUERY_PRODUCT: {
      // check it is a valid string
      bool valid = false;
      for (int i = 0; i < len; i++) {
        if (buffer[i] == 0x00) {
          valid = true;
          break;
        }
      }
      if (valid) {
        ESP_LOGD(TAG, "Tuya Product Code: %s", reinterpret_cast<const char *>(buffer));
      }
      break;
    }
    case TuyaCommandType::MCU_CONF:
      if (len >= 2) {
        gpio_status_ = buffer[0];
        gpio_reset_ = buffer[1];
      }
      // set wifi state LED to off or on depending on the MCU firmware
      // but it shouldn't be blinking
      c = 0x3;
      this->send_command_(TuyaCommandType::WIFI_STATE, &c, 1);
      this->send_empty_command_(TuyaCommandType::QUERY_STATE);
      break;
    case TuyaCommandType::WIFI_STATE:
      break;
    case TuyaCommandType::WIFI_RESET:
      ESP_LOGE(TAG, "TUYA_CMD_WIFI_RESET is not handled");
      break;
    case TuyaCommandType::WIFI_SELECT:
      ESP_LOGE(TAG, "TUYA_CMD_WIFI_SELECT is not handled");
      break;
    case TuyaCommandType::SET_DATAPOINT:
      break;
    case TuyaCommandType::STATE: {
      this->handle_datapoint_(buffer, len);
      break;
    }
    case TuyaCommandType::QUERY_STATE:
      break;
    default:
      ESP_LOGE(TAG, "invalid command (%02x) received", command);
  }
}

void Tuya::handle_datapoint_(const uint8_t *buffer, size_t len) {
  if (len < 2)
    return;

  TuyaDatapoint datapoint{};
  datapoint.id = buffer[0];
  datapoint.type = (TuyaDatapointType) buffer[1];
  datapoint.value_uint = 0;

  size_t data_size = (buffer[2] << 8) + buffer[3];
  const uint8_t *data = buffer + 4;
  size_t data_len = len - 4;
  if (data_size != data_len) {
    ESP_LOGW(TAG, "invalid datapoint update");
    return;
  }

  switch (datapoint.type) {
    case TuyaDatapointType::BOOLEAN:
      if (data_len != 1)
        return;
      datapoint.value_bool = data[0];
      break;
    case TuyaDatapointType::INTEGER:
      if (data_len != 4)
        return;
      datapoint.value_uint =
          (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | (uint32_t(data[3]) << 0);
      break;
    case TuyaDatapointType::ENUM:
      if (data_len != 1)
        return;
      datapoint.value_enum = data[0];
      break;
    case TuyaDatapointType::BITMASK:
      if (data_len != 2)
        return;
      datapoint.value_bitmask = (uint16_t(data[0]) << 8) | (uint16_t(data[1]) << 0);
      break;
    default:
      return;
  }
  ESP_LOGV(TAG, "Datapoint %u update to %u", datapoint.id, datapoint.value_uint);

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
    // New datapoint found, reprint dump_config after a delay.
    this->set_timeout("datapoint_dump", 100, [this] { this->dump_config(); });
  }

  // Run through listeners
  for (auto &listener : this->listeners_)
    if (listener.datapoint_id == datapoint.id)
      listener.on_datapoint(datapoint);
}

void Tuya::send_command_(TuyaCommandType command, const uint8_t *buffer, uint16_t len) {
  uint8_t len_hi = len >> 8;
  uint8_t len_lo = len >> 0;
  this->write_array({0x55, 0xAA,
                     0x00,  // version
                     (uint8_t) command, len_hi, len_lo});
  if (len != 0)
    this->write_array(buffer, len);

  uint8_t checksum = 0x55 + 0xAA + (uint8_t) command + len_hi + len_lo;
  for (int i = 0; i < len; i++)
    checksum += buffer[i];
  this->write_byte(checksum);
}

void Tuya::set_datapoint_value(TuyaDatapoint datapoint) {
  std::vector<uint8_t> buffer;
  ESP_LOGV(TAG, "Datapoint %u set to %u", datapoint.id, datapoint.value_uint);
  for (auto &other : this->datapoints_) {
    if (other.id == datapoint.id) {
      if (other.value_uint == datapoint.value_uint) {
        ESP_LOGV(TAG, "Not sending unchanged value");
        return;
      }
    }
  }
  buffer.push_back(datapoint.id);
  buffer.push_back(static_cast<uint8_t>(datapoint.type));

  std::vector<uint8_t> data;
  switch (datapoint.type) {
    case TuyaDatapointType::BOOLEAN:
      data.push_back(datapoint.value_bool);
      break;
    case TuyaDatapointType::INTEGER:
      data.push_back(datapoint.value_uint >> 24);
      data.push_back(datapoint.value_uint >> 16);
      data.push_back(datapoint.value_uint >> 8);
      data.push_back(datapoint.value_uint >> 0);
      break;
    case TuyaDatapointType::ENUM:
      data.push_back(datapoint.value_enum);
      break;
    case TuyaDatapointType::BITMASK:
      data.push_back(datapoint.value_bitmask >> 8);
      data.push_back(datapoint.value_bitmask >> 0);
      break;
    default:
      return;
  }

  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());
  this->send_command_(TuyaCommandType::SET_DATAPOINT, buffer.data(), buffer.size());
}

void Tuya::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_)
    if (datapoint.id == datapoint_id)
      func(datapoint);
}

}  // namespace tuya
}  // namespace esphome
