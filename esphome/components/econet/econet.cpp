#include "econet.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet";

static const uint32_t RECEIVE_TIMEOUT = 100;
static const uint32_t REQUEST_DELAY = 100;

static const uint32_t WIFI_MODULE = 0x340;
static const uint32_t SMARTEC_TRANSLATOR = 0x1040;
static const uint32_t HEAT_PUMP_WATER_HEATER = 0x1280;
static const uint32_t ELECTRIC_TANK_WATER_HEATER = 0x1200;
static const uint32_t CONTROL_CENTER = 0x380;

static const uint8_t DST_ADR_POS = 0;
static const uint8_t SRC_ADR_POS = 5;
static const uint8_t LEN_POS = 10;
static const uint8_t COMMAND_POS = 13;

static const uint8_t MSG_HEADER_SIZE = 14;
static const uint8_t MSG_CRC_SIZE = 2;

static const uint8_t ACK = 6;
static const uint8_t READ_COMMAND = 30;   // 0x1E
static const uint8_t WRITE_COMMAND = 31;  // 0x1F

uint16_t gen_crc16(const uint8_t *data, uint16_t size) {
  uint16_t out = 0;
  int bits_read = 0, bit_flag;

  /* Sanity check: */
  if (data == nullptr) {
    return 0;
  }

  while (size > 0) {
    bit_flag = out >> 15;

    /* Get next bit: */
    out <<= 1;
    out |= (*data >> bits_read) & 1;  // item a) work from the least significant bits

    /* Increment bit counter: */
    bits_read++;
    if (bits_read > 7) {
      bits_read = 0;
      data++;
      size--;
    }

    /* Cycle check: */
    if (bit_flag) {
      out ^= 0x8005;
    }
  }

  // item b) "push out" the last 16 bits
  int i;
  for (i = 0; i < 16; ++i) {
    bit_flag = out >> 15;
    out <<= 1;
    if (bit_flag) {
      out ^= 0x8005;
    }
  }

  // item c) reverse the bits
  uint16_t crc = 0;
  i = 0x8000;
  int j = 0x0001;
  for (; i != 0; i >>= 1, j <<= 1) {
    if (i & out) {
      crc |= j;
    }
  }

  return crc;
}

// Converts 4 bytes to float
float bytes_to_float(const uint8_t *b) {
  uint8_t byte_array[] = {b[3], b[2], b[1], b[0]};
  float result;
  std::copy(reinterpret_cast<const char *>(&byte_array[0]), reinterpret_cast<const char *>(&byte_array[4]),
            reinterpret_cast<char *>(&result));
  return result;
}

uint32_t float_to_uint32(float f) {
  uint32_t fbits = 0;
  memcpy(&fbits, &f, sizeof fbits);
  return fbits;
}

// Converts 4 bytes to an address
uint32_t bytes_to_address(const uint8_t *b) { return ((b[0] & 0x7f) << 24) + (b[1] << 16) + (b[2] << 8) + b[3]; }

// Reverse of bytes_to_address
void address_to_bytes(uint32_t adr, std::vector<uint8_t> *data) {
  data->push_back(0x80);
  data->push_back(adr >> 16);
  data->push_back(adr >> 8);
  data->push_back(adr);
  data->push_back(0);
}

// Extracts strings in pdata separated by 0x00
void extract_obj_names(const uint8_t *pdata, uint8_t data_len, std::vector<std::string> *obj_names) {
  const uint8_t *start = pdata + 4;
  while (true) {
    // Look for the first occurrence of 0x00 in the remaining bytes
    size_t num = data_len - (start - pdata);
    const uint8_t *end = (const uint8_t *) memchr(start, 0, num);
    if (!end) {
      // Not found, so add all the remaining bytes and finish
      std::string s((const char *) start, num);
      obj_names->push_back(s);
      break;
    }
    // Add all bytes until the first occurrence of 0x00
    std::string s((const char *) start, end - start);
    obj_names->push_back(s);
    start = end + 1;
    // Skip all 0x00 bytes
    while (!*start) {
      start++;
    }
  }
}

// Reverse of extract_obj_names
void join_obj_names(const std::vector<std::string> &objects, std::vector<uint8_t> *data) {
  for (const auto &s : objects) {
    data->push_back(0);
    data->push_back(0);
    for (int j = 0; j < 8; j++) {
      data->push_back(j < s.length() ? s[j] : 0);
    }
  }
}

std::string trim_trailing_whitespace(const char *p, uint8_t len) {
  const char *endp = p + len - 1;
  while (*endp == ' ' || *endp == 0) {
    endp--;
  }
  std::string s(p, endp - p + 1);
  return s;
}

void Econet::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet:");
  this->check_uart_settings(38400);
  for (auto &kv : this->datapoints_) {
    switch (kv.second.type) {
      case EconetDatapointType::FLOAT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: float value (value: %f)", kv.first.c_str(), kv.second.value_float);
        break;
      case EconetDatapointType::TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: text value (value: %s)", kv.first.c_str(), kv.second.value_string.c_str());
        break;
      case EconetDatapointType::ENUM_TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: enum value (value: %d : %s)", kv.first.c_str(), kv.second.value_enum,
                      kv.second.value_string.c_str());
        break;
      case EconetDatapointType::RAW:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: raw value (value: %s)", kv.first.c_str(),
                      format_hex_pretty(kv.second.value_raw).c_str());
        break;
    }
  }
}

// Makes one request: either the first pending write request or a new read request.
void Econet::make_request_() {
  // Use the address learned from a previous WRITE_COMMAND if possible.
  uint32_t dst_adr = this->dst_adr_;
  if (!dst_adr) {
    if (model_type_ == MODEL_TYPE_HEATPUMP) {
      dst_adr = HEAT_PUMP_WATER_HEATER;
    } else if (model_type_ == MODEL_TYPE_ELECTRIC_TANK) {
      dst_adr = ELECTRIC_TANK_WATER_HEATER;
    } else if (model_type_ == MODEL_TYPE_HVAC) {
      dst_adr = CONTROL_CENTER;
    } else {
      dst_adr = SMARTEC_TRANSLATOR;
    }
  }

  uint32_t src_adr = WIFI_MODULE;

  if (!pending_writes_.empty()) {
    const auto &kv = pending_writes_.begin();
    switch (kv->second.type) {
      case EconetDatapointType::FLOAT:
        this->write_value_(dst_adr, src_adr, kv->first, EconetDatapointType::FLOAT, kv->second.value_float);
        break;
      case EconetDatapointType::ENUM_TEXT:
        this->write_value_(dst_adr, src_adr, kv->first, EconetDatapointType::ENUM_TEXT, kv->second.value_enum);
        break;
      case EconetDatapointType::TEXT:
      case EconetDatapointType::RAW:
        ESP_LOGW(TAG, "Unexpected pending write: datapoint %s", kv->first.c_str());
        break;
    }
    pending_confirmation_writes_[kv->first] = kv->second;
    pending_writes_.erase(kv->first);
    return;
  }

  std::vector<std::string> str_ids(datapoint_ids_.begin(), datapoint_ids_.end());
  request_strings_(dst_adr, src_adr, str_ids);
}

void Econet::parse_tx_message_() { this->parse_message_(true); }

void Econet::parse_rx_message_() { this->parse_message_(false); }

void Econet::parse_message_(bool is_tx) {
  const uint8_t *b = is_tx ? &tx_message_[0] : &rx_message_[0];

  uint32_t dst_adr = bytes_to_address(b + DST_ADR_POS);
  uint32_t src_adr = bytes_to_address(b + SRC_ADR_POS);
  uint8_t data_len = b[LEN_POS];
  uint8_t command = b[COMMAND_POS];
  const uint8_t *pdata = b + MSG_HEADER_SIZE;

  ESP_LOGI(TAG, "%s %s", is_tx ? ">>>" : "<<<",
           format_hex_pretty(b, MSG_HEADER_SIZE + data_len + MSG_CRC_SIZE).c_str());
  ESP_LOGI(TAG, "  Dst Adr : 0x%x", dst_adr);
  ESP_LOGI(TAG, "  Src Adr : 0x%x", src_adr);
  ESP_LOGI(TAG, "  Command : %d", command);
  ESP_LOGI(TAG, "  Data    : %s", format_hex_pretty(pdata, data_len).c_str());

  uint16_t crc = (b[MSG_HEADER_SIZE + data_len]) + (b[MSG_HEADER_SIZE + data_len + 1] << 8);
  uint16_t crc_check = gen_crc16(b, MSG_HEADER_SIZE + data_len);
  if (crc != crc_check) {
    ESP_LOGW(TAG, "Ignoring message with incorrect crc");
    return;
  }

  // Track Read Requests
  if (command == READ_COMMAND) {
    EconetDatapointType type = EconetDatapointType(pdata[0] & 0x7F);
    uint8_t prop_type = pdata[1];

    ESP_LOGI(TAG, "  Type    : %hhu", type);
    ESP_LOGI(TAG, "  PropType: %hhu", prop_type);

    if (type != EconetDatapointType::TEXT && type != EconetDatapointType::ENUM_TEXT) {
      ESP_LOGI(TAG, "  Don't Currently Support This Class Type %hhu", type);
      return;
    }
    if (prop_type != 1) {
      ESP_LOGI(TAG, "  Don't Currently Support This Property Type %hhu", prop_type);
      return;
    }

    std::vector<std::string> obj_names;
    extract_obj_names(pdata, data_len, &obj_names);
    for (auto &obj_name : obj_names) {
      ESP_LOGI(TAG, "  %s", obj_name.c_str());
    }
    if (!obj_names.empty()) {
      read_req_.dst_adr = dst_adr;
      read_req_.src_adr = src_adr;
      read_req_.obj_names = obj_names;
      read_req_.awaiting_res = true;
    }

  } else if (command == ACK) {
    if (read_req_.dst_adr == src_adr && read_req_.src_adr == dst_adr && read_req_.awaiting_res) {
      if (read_req_.obj_names.size() == 1) {
        EconetDatapointType item_type = EconetDatapointType(pdata[0] & 0x7F);
        if (item_type == EconetDatapointType::RAW) {
          std::vector<uint8_t> raw(pdata, pdata + data_len);
          const std::string &datapoint_id = read_req_.obj_names[0];
          this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_raw = raw});
        }
      } else {
        int tpos = 0;
        uint8_t item_num = 0;

        while (tpos < data_len && item_num < read_req_.obj_names.size()) {
          uint8_t item_len = pdata[tpos];
          EconetDatapointType item_type = EconetDatapointType(pdata[tpos + 1] & 0x7F);
          const std::string &datapoint_id = read_req_.obj_names[item_num];

          if (item_type == EconetDatapointType::FLOAT && tpos + 7 < data_len) {
            float item_value = bytes_to_float(pdata + tpos + 4);
            ESP_LOGI(TAG, "  %s : %f", datapoint_id.c_str(), item_value);
            this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_float = item_value});
          } else if (item_type == EconetDatapointType::TEXT && tpos + 4 < data_len) {
            uint8_t item_text_len = item_len - 4;
            if (item_text_len > 0 && tpos + 4 + item_text_len < data_len) {
              std::string s = trim_trailing_whitespace((const char *) pdata + tpos + 4, item_text_len);
              ESP_LOGI(TAG, "  %s : (%s)", datapoint_id.c_str(), s.c_str());
              this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_string = s});
            }
          } else if (item_type == EconetDatapointType::ENUM_TEXT && tpos + 5 < data_len) {
            uint8_t item_value = pdata[tpos + 4];
            uint8_t item_text_len = pdata[tpos + 5];
            if (item_text_len > 0 && tpos + 6 + item_text_len < data_len) {
              std::string s = trim_trailing_whitespace((const char *) pdata + tpos + 6, item_text_len);
              ESP_LOGI(TAG, "  %s : %d (%s)", datapoint_id.c_str(), item_value, s.c_str());
              this->send_datapoint_(datapoint_id,
                                    EconetDatapoint{.type = item_type, .value_enum = item_value, .value_string = s});
            }
          }
          tpos += item_len + 1;
          item_num++;
        }
      }
      read_req_.awaiting_res = false;
    }
  } else if (command == WRITE_COMMAND) {
    // Update the address to use for subsequent requests.
    this->dst_adr_ = src_adr;
  }
}

void Econet::read_buffer_(int bytes_available) {
  uint8_t bytes[bytes_available];

  if (!this->read_array(bytes, bytes_available)) {
    return;
  }

  for (int i = 0; i < bytes_available; i++) {
    uint8_t byte = bytes[i];
    rx_message_.push_back(byte);
    uint32_t pos = rx_message_.size() - 1;
    if ((pos == DST_ADR_POS || pos == SRC_ADR_POS) && byte != 0x80) {
      rx_message_.clear();
      continue;
    }

    if (!rx_message_.empty() && rx_message_.size() > LEN_POS &&
        rx_message_.size() == MSG_HEADER_SIZE + rx_message_[LEN_POS] + MSG_CRC_SIZE) {
      // We have a full message
      this->parse_rx_message_();
      rx_message_.clear();
    }
  }
}

void Econet::loop() {
  const uint32_t now = millis();

  if ((now - this->last_read_data_ > RECEIVE_TIMEOUT) && !rx_message_.empty()) {
    ESP_LOGW(TAG, "Ignoring partially received message due to timeout");
    rx_message_.clear();
  }

  // Read Everything that is in the buffer
  int bytes_available = this->available();
  if (bytes_available > 0) {
    this->last_read_data_ = now;
    ESP_LOGI(TAG, "Read %d. ms=%d", bytes_available, now);
    this->read_buffer_(bytes_available);
    return;
  }

  if (!rx_message_.empty()) {
    ESP_LOGD(TAG, "Waiting to fully receive a partially received message");
    return;
  }

  if (now - this->last_request_ <= REQUEST_DELAY) {
    return;
  }

  // Quickly send writes but delay reads.
  if (!pending_writes_.empty() || !pending_confirmation_writes_.empty() ||
      (now - this->last_request_ > this->update_interval_millis_)) {
    ESP_LOGI(TAG, "request ms=%d", now);
    this->last_request_ = now;
    this->make_request_();
  }
}

void Econet::write_value_(uint32_t dst_adr, uint32_t src_adr, const std::string &object, EconetDatapointType type,
                          float value) {
  std::vector<uint8_t> data;

  data.push_back(1);
  data.push_back(1);
  data.push_back((uint8_t) type);
  data.push_back(1);
  data.push_back(0);
  data.push_back(0);

  for (int j = 0; j < 8; j++) {
    data.push_back(j < object.length() ? object[j] : 0);
  }

  uint32_t f_to_32 = float_to_uint32(value);

  data.push_back((uint8_t) (f_to_32 >> 24));
  data.push_back((uint8_t) (f_to_32 >> 16));
  data.push_back((uint8_t) (f_to_32 >> 8));
  data.push_back((uint8_t) (f_to_32));

  transmit_message_(dst_adr, src_adr, WRITE_COMMAND, data);
}

void Econet::request_strings_(uint32_t dst_adr, uint32_t src_adr, const std::vector<std::string> &objects) {
  std::vector<uint8_t> data;

  if (objects.size() > 1) {
    // Read Class
    data.push_back(2);
  } else {
    data.push_back(1);
  }

  // Read Property
  data.push_back(1);

  join_obj_names(objects, &data);

  transmit_message_(dst_adr, src_adr, READ_COMMAND, data);
}

void Econet::transmit_message_(uint32_t dst_adr, uint32_t src_adr, uint8_t command, const std::vector<uint8_t> &data) {
  tx_message_.clear();

  address_to_bytes(dst_adr, &tx_message_);
  address_to_bytes(src_adr, &tx_message_);

  tx_message_.push_back(data.size());
  tx_message_.push_back(0);
  tx_message_.push_back(0);
  tx_message_.push_back(command);
  tx_message_.insert(tx_message_.end(), data.begin(), data.end());

  uint16_t crc = gen_crc16(&tx_message_[0], tx_message_.size());
  tx_message_.push_back(crc);
  tx_message_.push_back(crc >> 8);

  this->write_array(&tx_message_[0], tx_message_.size());
  // this->flush();

  parse_tx_message_();
}

void Econet::set_float_datapoint_value(const std::string &datapoint_id, float value) {
  ESP_LOGD(TAG, "Setting datapoint %s to %f", datapoint_id.c_str(), value);
  this->set_datapoint_(datapoint_id, EconetDatapoint{.type = EconetDatapointType::FLOAT, .value_float = value});
}

void Econet::set_enum_datapoint_value(const std::string &datapoint_id, uint8_t value) {
  ESP_LOGD(TAG, "Setting datapoint %s to %u", datapoint_id.c_str(), value);
  this->set_datapoint_(datapoint_id, EconetDatapoint{.type = EconetDatapointType::ENUM_TEXT, .value_enum = value});
}

void Econet::set_datapoint_(const std::string &datapoint_id, const EconetDatapoint &value) {
  if (datapoints_.count(datapoint_id) == 0) {
    ESP_LOGW(TAG, "Setting unknown datapoint %s", datapoint_id.c_str());
  } else {
    EconetDatapoint old_value = datapoints_[datapoint_id];
    if (old_value.type != value.type) {
      ESP_LOGE(TAG, "Attempt to set datapoint %s with incorrect type", datapoint_id.c_str());
      return;
    } else if (old_value == value && pending_writes_.count(datapoint_id) == 0) {
      ESP_LOGV(TAG, "Not setting unchanged value for datapoint %s", datapoint_id.c_str());
      return;
    }
  }
  pending_writes_[datapoint_id] = value;
  if (rx_message_.empty()) {
    make_request_();
  }
  send_datapoint_(datapoint_id, value, true);
}

void Econet::send_datapoint_(const std::string &datapoint_id, const EconetDatapoint &value, bool skip_update_state) {
  if (!skip_update_state) {
    if (pending_confirmation_writes_.count(datapoint_id) == 1) {
      if (value == pending_confirmation_writes_[datapoint_id]) {
        ESP_LOGV(TAG, "Confirmed write for datapoint %s", datapoint_id.c_str());
      } else {
        ESP_LOGW(TAG, "Retrying write for datapoint %s", datapoint_id.c_str());
        pending_writes_[datapoint_id] = pending_confirmation_writes_[datapoint_id];
      }
      pending_confirmation_writes_.erase(datapoint_id);
    }
    if (datapoints_.count(datapoint_id) == 1) {
      EconetDatapoint old_value = datapoints_[datapoint_id];
      if (old_value == value) {
        ESP_LOGV(TAG, "Not sending unchanged value for datapoint %s", datapoint_id.c_str());
        return;
      }
    }
    datapoints_[datapoint_id] = value;
  }
  for (auto &listener : this->listeners_) {
    if (listener.datapoint_id == datapoint_id) {
      listener.on_datapoint(value);
    }
  }
}

void Econet::register_listener(const std::string &datapoint_id, const std::function<void(EconetDatapoint)> &func,
                               bool is_raw_datapoint) {
  // Don't issue a READ_COMMAND in request_strings_ for RAW datapoints. These need to be requested separately.
  // For now rely on other devices, e.g. thermostat, requesting them.
  if (!is_raw_datapoint) {
    datapoint_ids_.insert(datapoint_id);
  }
  auto listener = EconetDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &kv : this->datapoints_) {
    if (kv.first == datapoint_id) {
      func(kv.second);
    }
  }
}

}  // namespace econet
}  // namespace esphome
