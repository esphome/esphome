#include "alpha3.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <lwip/sockets.h>  //gives ntohl

#ifdef USE_ESP32

namespace esphome {
namespace alpha3 {

static const char *const TAG = "alpha3";

void Alpha3::dump_config() {
  ESP_LOGCONFIG(TAG, "ALPHA3");
  LOG_SENSOR(" ", "Flow", this->flow_sensor_);
  LOG_SENSOR(" ", "Head", this->head_sensor_);
  LOG_SENSOR(" ", "Power", this->power_sensor_);
  LOG_SENSOR(" ", "Current", this->current_sensor_);
  LOG_SENSOR(" ", "Speed", this->speed_sensor_);
  LOG_SENSOR(" ", "Voltage", this->voltage_sensor_);
}

void Alpha3::setup() {}

void Alpha3::extract_publish_sensor_value_(const uint8_t *response, int16_t length, int16_t response_offset,
                                           int16_t value_offset, sensor::Sensor *sensor, float factor) {
  if (sensor == nullptr)
    return;
  // we need to handle cases where a value is split over two packets
  const int16_t value_length = 4;  // 32bit float
  // offset inside current response packet
  auto rel_offset = value_offset - response_offset;
  if (rel_offset <= -value_length)
    return;  // aready passed the value completly
  if (rel_offset >= length)
    return;  // value not in this packet

  auto start_offset = std::max(0, rel_offset);
  auto end_offset = std::min((int16_t) (rel_offset + value_length), length);
  auto copy_length = end_offset - start_offset;
  auto buffer_offset = std::max(-rel_offset, 0);
  std::memcpy(this->buffer_ + buffer_offset, response + start_offset, copy_length);

  if (rel_offset + value_length <= length) {
    // we have the whole value
    void *buffer = this->buffer_;                          // to prevent warnings when casting the pointer
    *((int32_t *) buffer) = ntohl(*((int32_t *) buffer));  // values are big endian
    float fvalue = *((float *) buffer);
    sensor->publish_state(fvalue * factor);
  }
}

bool Alpha3::is_current_response_type_(const uint8_t *response_type) {
  return !std::memcmp(this->response_type_, response_type, GENI_RESPONSE_TYPE_LENGTH);
}

void Alpha3::handle_geni_response_(const uint8_t *response, uint16_t length) {
  if (this->response_offset_ >= this->response_length_) {
    ESP_LOGD(TAG, "[%s] GENI response begin", this->parent_->address_str());
    if (length < GENI_RESPONSE_HEADER_LENGTH) {
      ESP_LOGW(TAG, "[%s] response too short", this->parent_->address_str());
      return;
    }
    if (response[0] != 36 || response[2] != 248 || response[3] != 231 || response[4] != 10) {
      ESP_LOGW(TAG, "[%s] response bytes %d %d %d %d %d don't match GENI HEADER", this->parent_->address_str(),
               response[0], response[1], response[2], response[3], response[4]);
      return;
    }
    this->response_length_ = response[1] - GENI_RESPONSE_HEADER_LENGTH + 2;  // maybe 2 byte checksum
    this->response_offset_ = -GENI_RESPONSE_HEADER_LENGTH;
    std::memcpy(this->response_type_, response + 5, GENI_RESPONSE_TYPE_LENGTH);
  }

  auto extract_publish_sensor_value = [response, length, this](int16_t value_offset, sensor::Sensor *sensor,
                                                               float factor) {
    this->extract_publish_sensor_value_(response, length, this->response_offset_, value_offset, sensor, factor);
  };

  if (this->is_current_response_type_(GENI_RESPONSE_TYPE_FLOW_HEAD)) {
    ESP_LOGD(TAG, "[%s] FLOW HEAD Response", this->parent_->address_str());
    extract_publish_sensor_value(GENI_RESPONSE_FLOW_OFFSET, this->flow_sensor_, 3600.0F);
    extract_publish_sensor_value(GENI_RESPONSE_HEAD_OFFSET, this->head_sensor_, .0001F);
  } else if (this->is_current_response_type_(GENI_RESPONSE_TYPE_POWER)) {
    ESP_LOGD(TAG, "[%s] POWER Response", this->parent_->address_str());
    extract_publish_sensor_value(GENI_RESPONSE_POWER_OFFSET, this->power_sensor_, 1.0F);
    extract_publish_sensor_value(GENI_RESPONSE_CURRENT_OFFSET, this->current_sensor_, 1.0F);
    extract_publish_sensor_value(GENI_RESPONSE_MOTOR_SPEED_OFFSET, this->speed_sensor_, 1.0F);
    extract_publish_sensor_value(GENI_RESPONSE_VOLTAGE_AC_OFFSET, this->voltage_sensor_, 1.0F);
  } else {
    ESP_LOGW(TAG, "unknown GENI response Type %d %d %d %d %d %d %d %d", this->response_type_[0],
             this->response_type_[1], this->response_type_[2], this->response_type_[3], this->response_type_[4],
             this->response_type_[5], this->response_type_[6], this->response_type_[7]);
  }
  this->response_offset_ += length;
}

void Alpha3::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        this->response_offset_ = 0;
        this->response_length_ = 0;
        ESP_LOGI(TAG, "[%s] connection open", this->parent_->address_str());
      }
      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      if (std::memcmp(param->connect.remote_bda, this->parent_->get_remote_bda(), 6) != 0)
        return;
      auto ret = esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
      if (ret) {
        ESP_LOGW(TAG, "esp_ble_set_encryption failed, status=%x", ret);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      this->node_state = espbt::ClientState::IDLE;
      if (this->flow_sensor_ != nullptr)
        this->flow_sensor_->publish_state(NAN);
      if (this->head_sensor_ != nullptr)
        this->head_sensor_->publish_state(NAN);
      if (this->power_sensor_ != nullptr)
        this->power_sensor_->publish_state(NAN);
      if (this->current_sensor_ != nullptr)
        this->current_sensor_->publish_state(NAN);
      if (this->speed_sensor_ != nullptr)
        this->speed_sensor_->publish_state(NAN);
      if (this->speed_sensor_ != nullptr)
        this->voltage_sensor_->publish_state(NAN);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(ALPHA3_GENI_SERVICE_UUID, ALPHA3_GENI_CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        ESP_LOGE(TAG, "[%s] No GENI service found at device, not an Alpha3..?", this->parent_->address_str());
        break;
      }
      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      this->geni_handle_ = chr->handle;
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->update();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->geni_handle_) {
        this->handle_geni_response_(param->notify.value, param->notify.value_len);
      }
      break;
    }
    default:
      break;
  }
}

void Alpha3::send_request_(uint8_t *request, size_t len) {
  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->geni_handle_, len,
                               request, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status)
    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str(), status);
}

uint16_t Alpha3::calculate_crc_(const uint8_t *data, size_t len) {
  // CRC-CCITT (polynomial 0x1021) with initial value 0xFFFF and final XOR 0xFFFF
  // CRC is calculated from byte index 1 onwards (skips start delimiter)
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc ^ 0xFFFF;
}

void Alpha3::send_command_(uint8_t command_id) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot send command, not connected", this->parent_->address_str());
    return;
  }

  uint8_t packet[10];
  packet[0] = 0x27;  // Start delimiter (DATA_REQUEST)
  packet[1] = 0x06;  // Length (6 bytes until CRC)
  packet[2] = 0xF8;  // Dest address high (from response header check)
  packet[3] = 0xE7;  // Dest address low
  packet[4] = 0x0A;  // Source address (our address)
  packet[5] = 0x03;  // APDU Class: COMMANDS
  packet[6] = 0x81;  // Operation: SET (2 << 6) | num_commands (1)
  packet[7] = command_id;

  // Calculate CRC from byte 1 onwards
  uint16_t crc = this->calculate_crc_(packet + 1, 7);
  packet[8] = crc >> 8;
  packet[9] = crc & 0xFF;

  ESP_LOGD(TAG, "[%s] Sending command ID %d", this->parent_->address_str(), command_id);
  this->send_request_(packet, sizeof(packet));
}

void Alpha3::send_commands_(const uint8_t *command_ids, size_t num_commands) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot send commands, not connected", this->parent_->address_str());
    return;
  }

  if (num_commands == 0 || num_commands > 10) {
    ESP_LOGW(TAG, "[%s] Invalid number of commands: %zu", this->parent_->address_str(), num_commands);
    return;
  }

  uint8_t packet[20];
  packet[0] = 0x27;                                       // Start delimiter
  packet[1] = 5 + num_commands;                           // Length
  packet[2] = 0xF8;                                       // Dest high
  packet[3] = 0xE7;                                       // Dest low
  packet[4] = 0x0A;                                       // Source
  packet[5] = 0x03;                                       // Class: COMMANDS
  packet[6] = 0x80 | static_cast<uint8_t>(num_commands);  // SET | num_commands

  // Copy command IDs
  for (size_t i = 0; i < num_commands; i++) {
    packet[7 + i] = command_ids[i];
  }

  // Calculate CRC
  size_t crc_offset = 7 + num_commands;
  uint16_t crc = this->calculate_crc_(packet + 1, crc_offset - 1);
  packet[crc_offset] = crc >> 8;
  packet[crc_offset + 1] = crc & 0xFF;

  ESP_LOGD(TAG, "[%s] Sending %zu commands", this->parent_->address_str(), num_commands);
  this->send_request_(packet, crc_offset + 2);
}

void Alpha3::send_command(uint8_t command_id) { this->send_command_(command_id); }

void Alpha3::set_remote_mode() {
  ESP_LOGI(TAG, "[%s] Switching to remote mode", this->parent_->address_str());
  this->send_command_(GENI_CMD_REMOTE);
}

void Alpha3::set_local_mode() {
  ESP_LOGI(TAG, "[%s] Switching to local mode", this->parent_->address_str());
  this->send_command_(GENI_CMD_LOCAL);
}

void Alpha3::set_mode_autoadapt() {
  ESP_LOGI(TAG, "[%s] Setting mode to AutoAdapt", this->parent_->address_str());
  const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_AUTOADAPT};
  this->send_commands_(commands, 2);
}

void Alpha3::set_mode_const_pressure(uint8_t level) {
  ESP_LOGI(TAG, "[%s] Setting mode to Constant Pressure level %d", this->parent_->address_str(), level);
  if (level == 1) {
    const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_CONST_PRESS, GENI_CMD_MIN};
    this->send_commands_(commands, 3);
  } else if (level == 3) {
    const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_CONST_PRESS, GENI_CMD_MAX};
    this->send_commands_(commands, 3);
  } else {
    const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_CONST_PRESS};
    this->send_commands_(commands, 2);
  }
}

void Alpha3::set_mode_prop_pressure(uint8_t level) {
  ESP_LOGI(TAG, "[%s] Setting mode to Proportional Pressure level %d", this->parent_->address_str(), level);
  if (level == 1) {
    const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_PROP_PRESS, GENI_CMD_MIN};
    this->send_commands_(commands, 3);
  } else if (level == 3) {
    const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_PROP_PRESS, GENI_CMD_MAX};
    this->send_commands_(commands, 3);
  } else {
    const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_PROP_PRESS};
    this->send_commands_(commands, 2);
  }
}

void Alpha3::set_mode_const_freq() {
  ESP_LOGI(TAG, "[%s] Setting mode to Constant Frequency", this->parent_->address_str());
  const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_CONST_FREQ};
  this->send_commands_(commands, 2);
}

void Alpha3::stop_pump() {
  ESP_LOGI(TAG, "[%s] Stopping pump", this->parent_->address_str());
  const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_STOP};
  this->send_commands_(commands, 2);
}

void Alpha3::start_pump() {
  ESP_LOGI(TAG, "[%s] Starting pump", this->parent_->address_str());
  const uint8_t commands[] = {GENI_CMD_REMOTE, GENI_CMD_START};
  this->send_commands_(commands, 2);
}

void Alpha3::adjust_setpoint(int8_t delta) {
  if (delta > 0) {
    ESP_LOGI(TAG, "[%s] Increasing setpoint", this->parent_->address_str());
    this->send_command_(GENI_CMD_REF_UP);
  } else if (delta < 0) {
    ESP_LOGI(TAG, "[%s] Decreasing setpoint", this->parent_->address_str());
    this->send_command_(GENI_CMD_REF_DOWN);
  }
}

void Alpha3::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->parent_->address_str());
    return;
  }

  if (this->flow_sensor_ != nullptr || this->head_sensor_ != nullptr) {
    uint8_t geni_request_flow_head[] = {39, 7, 231, 248, 10, 3, 93, 1, 33, 82, 31};
    this->send_request_(geni_request_flow_head, sizeof(geni_request_flow_head));
  }
  if (this->power_sensor_ != nullptr || this->current_sensor_ != nullptr || this->speed_sensor_ != nullptr ||
      this->voltage_sensor_ != nullptr) {
    uint8_t geni_request_power[] = {39, 7, 231, 248, 10, 3, 87, 0, 69, 138, 205};
    this->send_request_(geni_request_power, sizeof(geni_request_power));
  }
}

const char *Alpha3::get_mode_name_(uint8_t mode) {
  // Mode values from act_mode1 byte (GENIBus protocol)
  // These may differ from the Alpha Reader broadcast protocol
  switch (mode) {
    case 0x00:
      return "Constant Speed 3";
    case 0x01:
      return "Constant Speed 2";
    case 0x02:
      return "Constant Speed 1";
    case 0x03:
      return "AutoAdapt";
    case 0x04:
      return "Proportional Pressure 1";
    case 0x05:
      return "Proportional Pressure 2";
    case 0x06:
      return "Proportional Pressure 3";
    case 0x07:
      return "Constant Pressure 1";
    case 0x08:
      return "Constant Pressure 2";
    case 0x09:
      return "Constant Pressure 3";
    default:
      return "Unknown";
  }
}
}  // namespace alpha3
}  // namespace esphome

#endif
