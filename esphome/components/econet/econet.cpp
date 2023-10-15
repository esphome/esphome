#include "econet.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet";

static const uint32_t RECEIVE_TIMEOUT = 100;
static const uint32_t REQUEST_DELAY = 100;

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
  while ((*endp == ' ' || *endp == 0) && endp != p - 1) {
    endp--;
  }
  std::string s(p, endp - p + 1);
  return s;
}

void Econet::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet:");
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
  if (!pending_writes_.empty()) {
    const auto &kv = pending_writes_.begin();
    switch (kv->second.type) {
      case EconetDatapointType::FLOAT:
        this->write_value_(kv->first, EconetDatapointType::FLOAT, kv->second.value_float);
        break;
      case EconetDatapointType::ENUM_TEXT:
        this->write_value_(kv->first, EconetDatapointType::ENUM_TEXT, kv->second.value_enum);
        break;
      case EconetDatapointType::TEXT:
      case EconetDatapointType::RAW:
        ESP_LOGW(TAG, "Unexpected pending write: datapoint %s", kv->first.c_str());
        break;
    }
    pending_writes_.erase(kv->first);
    return;
  }

  request_strings_();
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
    uint8_t type = pdata[0] & 0x7F;
    uint8_t prop_type = pdata[1];

    ESP_LOGI(TAG, "  Type    : %hhu", type);
    ESP_LOGI(TAG, "  PropType: %hhu", prop_type);

    if (type != 1 && type != 2) {
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
      read_req_.type = type;
      read_req_.obj_names = obj_names;
      read_req_.awaiting_res = true;
    }

  } else if (command == ACK) {
    if (read_req_.dst_adr == src_adr && read_req_.src_adr == dst_adr && read_req_.awaiting_res) {
      if (read_req_.type == 1 && read_req_.obj_names.size() == 1) {
        EconetDatapointType item_type = EconetDatapointType(pdata[0] & 0x7F);
        if (item_type == EconetDatapointType::RAW) {
          std::vector<uint8_t> raw(pdata, pdata + data_len);
          const std::string &datapoint_id = read_req_.obj_names[0];
          this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_raw = raw});
        }
      } else if (read_req_.type == 2) {
        int tpos = 0;
        uint8_t item_num = 0;

        while (tpos < data_len && item_num < read_req_.obj_names.size()) {
          const std::string &datapoint_id = read_req_.obj_names[item_num];
          uint8_t item_len = pdata[tpos];
          if (item_len <= 4 && tpos + item_len >= data_len) {
            ESP_LOGE(TAG, "Unexpected length of %d for %s", item_len, datapoint_id.c_str());
            break;
          }
          EconetDatapointType item_type = EconetDatapointType(pdata[tpos + 1] & 0x7F);
          handle_response_(datapoint_id, item_type, pdata + tpos + 4, item_len - 4 + 1);
          tpos += item_len + 1;
          item_num++;
        }
      }
      read_req_.awaiting_res = false;
    }
  } else if (command == WRITE_COMMAND) {
    // Update the address to use for subsequent requests.
    this->dst_adr_ = src_adr;

    uint8_t type = pdata[0];
    ESP_LOGI(TAG, "  ClssType: %d", type);
    if (type == 1 && pdata[1] == 1) {
      EconetDatapointType datatype = EconetDatapointType(pdata[2]);
      if (datatype == EconetDatapointType::FLOAT || datatype == EconetDatapointType::ENUM_TEXT) {
        if (data_len == 18) {
          std::string s((const char *) pdata + 6, 8);
          float item_value = bytes_to_float(pdata + 6 + 8);
          ESP_LOGI(TAG, "  %s: %f", s.c_str(), item_value);
        } else {
          ESP_LOGI(TAG, "  Unexpected Write Data Length");
        }
      }
    } else if (type == 7) {
      ESP_LOGI(TAG, "  DateTime: %04d/%02d/%02d %02d:%02d:%02d.%02d\n", pdata[9] | pdata[8] << 8, pdata[7], pdata[6],
               pdata[5], pdata[4], pdata[3], pdata[2]);
    }
  }
}

void Econet::handle_response_(const std::string &datapoint_id, EconetDatapointType item_type, const uint8_t *p,
                              uint8_t len) {
  if (item_type == EconetDatapointType::FLOAT) {
    if (len != 4) {
      ESP_LOGE(TAG, "Expected len of 4 but was %d for %s", len, datapoint_id.c_str());
      return;
    }
    float item_value = bytes_to_float(p);
    ESP_LOGI(TAG, "  %s : %f", datapoint_id.c_str(), item_value);
    this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_float = item_value});
  } else if (item_type == EconetDatapointType::TEXT) {
    std::string s = trim_trailing_whitespace((const char *) p, len);
    ESP_LOGI(TAG, "  %s : (%s)", datapoint_id.c_str(), s.c_str());
    this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_string = s});
  } else if (item_type == EconetDatapointType::ENUM_TEXT) {
    if (len < 2) {
      ESP_LOGE(TAG, "Expected len of at least 2 but was %d for %s", len, datapoint_id.c_str());
      return;
    }
    uint8_t item_value = p[0];
    uint8_t item_text_len = p[1];
    if (item_text_len != len - 2) {
      ESP_LOGE(TAG, "Expected text len of %d but was %d for %s", len - 2, item_text_len, datapoint_id.c_str());
      return;
    }
    std::string s = trim_trailing_whitespace((const char *) p + 2, item_text_len);
    ESP_LOGI(TAG, "  %s : %d (%s)", datapoint_id.c_str(), item_value, s.c_str());
    this->send_datapoint_(datapoint_id,
                          EconetDatapoint{.type = item_type, .value_enum = item_value, .value_string = s});
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
  if (!pending_writes_.empty() || (now - this->last_request_ > this->update_interval_millis_ / request_mods_)) {
    ESP_LOGI(TAG, "request ms=%d", now);
    this->last_request_ = now;
    this->make_request_();
  }
}

void Econet::write_value_(const std::string &object, EconetDatapointType type, float value) {
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

  transmit_message_(WRITE_COMMAND, data);
}

void Econet::request_strings_() {
  uint8_t request_mod = read_requests_++ % request_mods_;
  std::vector<std::string> objects(request_datapoint_ids_[request_mod].begin(),
                                   request_datapoint_ids_[request_mod].end());
  std::vector<std::string>::iterator iter;
  for (iter = objects.begin(); iter != objects.end();) {
    if (request_once_datapoint_ids_.count(*iter) == 1 && datapoints_.count(*iter) == 1) {
      iter = objects.erase(iter);
    } else {
      ++iter;
    }
  }
  if (objects.empty()) {
    return;
  }

  std::vector<uint8_t> data;

  // Read Class
  if (objects.size() == 1 && raw_datapoint_ids_.count(objects[0]) == 1) {
    data.push_back(1);
  } else {
    data.push_back(2);
  }

  // Read Property
  data.push_back(1);

  join_obj_names(objects, &data);

  transmit_message_(READ_COMMAND, data);
}

void Econet::transmit_message_(uint8_t command, const std::vector<uint8_t> &data) {
  tx_message_.clear();

  address_to_bytes(dst_adr_, &tx_message_);
  address_to_bytes(src_adr_, &tx_message_);

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
  send_datapoint_(datapoint_id, value);
}

void Econet::send_datapoint_(const std::string &datapoint_id, const EconetDatapoint &value) {
  if (datapoints_.count(datapoint_id) == 1) {
    EconetDatapoint old_value = datapoints_[datapoint_id];
    if (old_value == value) {
      ESP_LOGV(TAG, "Not sending unchanged value for datapoint %s", datapoint_id.c_str());
      return;
    }
  }
  datapoints_[datapoint_id] = value;
  for (auto &listener : this->listeners_) {
    if (listener.datapoint_id == datapoint_id) {
      listener.on_datapoint(value);
    }
  }
}

void Econet::register_listener(const std::string &datapoint_id, int8_t request_mod, bool request_once,
                               const std::function<void(EconetDatapoint)> &func, bool is_raw_datapoint) {
  if (request_mod >= 0 && request_mod < request_datapoint_ids_.size()) {
    request_datapoint_ids_[request_mod].insert(datapoint_id);
    request_mods_ = std::max(request_mods_, (uint8_t) (request_mod + 1));
    if (request_once) {
      request_once_datapoint_ids_.insert(datapoint_id);
    }
  }
  if (is_raw_datapoint) {
    raw_datapoint_ids_.insert(datapoint_id);
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
