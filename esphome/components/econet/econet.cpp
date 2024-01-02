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

static const uint8_t OBJ_NAME_POS = 6;
static const uint8_t OBJ_NAME_SIZE = 8;
static const uint8_t WRITE_DATA_POS = OBJ_NAME_POS + OBJ_NAME_SIZE;
static const uint8_t FLOAT_SIZE = sizeof(float);

static const uint8_t MSG_HEADER_SIZE = 14;
static const uint8_t MSG_CRC_SIZE = 2;

static const uint8_t ACK = 6;
static const uint8_t READ_COMMAND = 30;   // 0x1E
static const uint8_t WRITE_COMMAND = 31;  // 0x1F

// Converts 4 bytes (FLOAT_SIZE) to float
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

// Extracts strings of length OBJ_NAME_SIZE in pdata separated by 0x00, 0x00
void extract_obj_names(const uint8_t *pdata, uint8_t data_len, std::vector<std::string> *obj_names) {
  const uint8_t *start = pdata + 4;
  const uint8_t *endp = pdata + data_len;
  while (start < endp) {
    const uint8_t *end = std::min(start + OBJ_NAME_SIZE, endp);
    std::string s((const char *) start, end - start);
    s.erase(remove(s.begin(), s.end(), '\00'), s.end());
    obj_names->push_back(s);
    start = end + 2;
  }
}

// Reverse of extract_obj_names
void join_obj_names(const std::vector<std::string> &objects, std::vector<uint8_t> *data) {
  for (const auto &s : objects) {
    data->push_back(0);
    data->push_back(0);
    for (int j = 0; j < OBJ_NAME_SIZE; j++) {
      data->push_back(j < s.length() ? s[j] : 0);
    }
  }
}

std::string trim_trailing_whitespace(const char *p, uint8_t len) {
  const char *endp = p + len - 1;
  while (endp >= p && (*endp == ' ' || *endp == 0)) {
    endp--;
  }
  std::string s(p, endp - p + 1);
  return s;
}

void Econet::setup() {
  if (flow_control_pin_ != nullptr) {
    flow_control_pin_->setup();
  }
}

void Econet::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  for (auto &kv : this->datapoints_) {
    switch (kv.second.type) {
      case EconetDatapointType::FLOAT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: float value (value: %f)", kv.first.name.c_str(), kv.second.value_float);
        break;
      case EconetDatapointType::TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: text value (value: %s)", kv.first.name.c_str(),
                      kv.second.value_string.c_str());
        break;
      case EconetDatapointType::ENUM_TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: enum value (value: %d : %s)", kv.first.name.c_str(), kv.second.value_enum,
                      kv.second.value_string.c_str());
        break;
      case EconetDatapointType::RAW:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: raw value (value: %s)", kv.first.name.c_str(),
                      format_hex_pretty(kv.second.value_raw).c_str());
        break;
      case EconetDatapointType::UNSUPPORTED:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: UNSUPPORTED", kv.first.name.c_str());
        break;
    }
  }
}

// Makes one request: either the first pending write request or a new read request.
void Econet::make_request_() {
  if (!pending_writes_.empty()) {
    const auto &kv = pending_writes_.begin();
    const auto &dpp = kv->first;
    uint32_t address = dpp.address;
    switch (kv->second.type) {
      case EconetDatapointType::FLOAT:
        this->write_value_(dpp.name, EconetDatapointType::FLOAT, kv->second.value_float, address);
        break;
      case EconetDatapointType::ENUM_TEXT:
        this->write_value_(dpp.name, EconetDatapointType::ENUM_TEXT, kv->second.value_enum, address);
        break;
      case EconetDatapointType::TEXT:
      case EconetDatapointType::RAW:
      case EconetDatapointType::UNSUPPORTED:
        ESP_LOGW(TAG, "Unexpected pending write: datapoint %s", dpp.name.c_str());
        break;
    }
    pending_writes_.erase(kv);
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
  uint16_t crc_check = crc16(b, MSG_HEADER_SIZE + data_len, 0);
  if (crc != crc_check) {
    read_req_.awaiting_res = false;
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

    if (read_req_.awaiting_res) {
      ESP_LOGW(TAG, "New read request while waiting response for previous read request");
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
          this->send_datapoint_(EconetDatapointID{.name = datapoint_id, .address = src_adr},
                                EconetDatapoint{.type = item_type, .value_raw = raw});
        }
      } else if (read_req_.type == 2) {
        // 1st pass to validate response and avoid any buffer over-read
        // We expect: read_req_.obj_names.size() sections where each section has 1 byte representing the item_len
        // followed by item_len bytes (see handle_response_).
        int tpos = 0;
        uint8_t item_num = 0;
        while (tpos < data_len) {
          uint8_t item_len = pdata[tpos];
          if (item_len == 0 || tpos + item_len >= data_len) {
            ESP_LOGE(TAG, "Unexpected item length of %d at position %d", item_len, tpos);
            break;
          }
          tpos += item_len + 1;
          item_num++;
        }
        if (item_num != read_req_.obj_names.size()) {
          ESP_LOGE(TAG, "We requested %d objects but we received %d. Ignoring response.", read_req_.obj_names.size(),
                   item_num);
        } else {
          // 2nd pass to handle response
          tpos = 0;
          item_num = 0;
          while (tpos < data_len) {
            const std::string &datapoint_id = read_req_.obj_names[item_num];
            uint8_t item_len = pdata[tpos];
            handle_response_(EconetDatapointID{.name = datapoint_id, .address = src_adr}, pdata + tpos + 1, item_len);
            tpos += item_len + 1;
            item_num++;
          }
        }
      }
      read_req_.awaiting_res = false;
    }
  } else if (command == WRITE_COMMAND) {
    uint8_t type = pdata[0];
    ESP_LOGI(TAG, "  ClssType: %d", type);
    if (type == 1 && pdata[1] == 1 && data_len >= WRITE_DATA_POS) {
      std::string item_name((const char *) pdata + OBJ_NAME_POS, OBJ_NAME_SIZE);
      switch (EconetDatapointType(pdata[2])) {
        case EconetDatapointType::FLOAT:
        case EconetDatapointType::ENUM_TEXT:
          if (data_len == WRITE_DATA_POS + FLOAT_SIZE) {
            float item_value = bytes_to_float(pdata + WRITE_DATA_POS);
            ESP_LOGI(TAG, "  %s: %f", item_name.c_str(), item_value);
          } else {
            ESP_LOGW(TAG, "  %s: Unexpected Write Data Length", item_name.c_str());
          }
          break;
        case EconetDatapointType::RAW:
          ESP_LOGI(TAG, "  %s: %s", item_name.c_str(),
                   format_hex_pretty(pdata + WRITE_DATA_POS, data_len - WRITE_DATA_POS).c_str());
          break;
        case EconetDatapointType::TEXT:
          ESP_LOGW(TAG, "(Please file an issue with the following line to add support for TEXT)");
          ESP_LOGW(TAG, "  %s: %s", item_name.c_str(), format_hex_pretty(pdata, data_len).c_str());
          break;
        case EconetDatapointType::UNSUPPORTED:
          ESP_LOGW(TAG, "  %s: UNSUPPORTED", item_name.c_str());
          break;
      }
    } else if (type == 7) {
      ESP_LOGI(TAG, "  DateTime: %04d/%02d/%02d %02d:%02d:%02d.%02d\n", pdata[9] | pdata[8] << 8, pdata[7], pdata[6],
               pdata[5], pdata[4], pdata[3], pdata[2]);
    } else if (type == 9) {
      if (this->dst_adr_ != src_adr) {
        ESP_LOGW(TAG, "Using 0x%x as dst_address from now on. File an issue if you see this more than once.", src_adr);
        this->dst_adr_ = src_adr;
      }
    }
  }
}

// len comes from p[-1], see caller. The caller is responsible for ensuring there won't be any buffer over-read.
// type is at p[0]. If it's a supported type there is always 3 bytes: 0x80, 0x00, 0x00 followed by the actual data.
// For FLOAT it's 4 bytes.
// For TEXT it's some predefined number of bytes depending on the requested object padded with trailing whitespace.
// For ENUM_TEXT it's 1 byte for the enum value, followed by one byte for the length of the enum text, and finally
// followed by the bytes of the enum text padded with trailing whitespace.
void Econet::handle_response_(EconetDatapointID datapoint_id, const uint8_t *p, uint8_t len) {
  EconetDatapointType item_type = EconetDatapointType(p[0] & 0x7F);
  switch (item_type) {
    case EconetDatapointType::FLOAT: {
      p += 3;
      len -= 3;
      if (len != FLOAT_SIZE) {
        ESP_LOGE(TAG, "Expected len of %d but was %d for %s", FLOAT_SIZE, len, datapoint_id.name.c_str());
        return;
      }
      float item_value = bytes_to_float(p);
      ESP_LOGI(TAG, "  %s : %f", datapoint_id.name.c_str(), item_value);
      this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_float = item_value});
      break;
    }
    case EconetDatapointType::TEXT: {
      p += 3;
      len -= 3;
      std::string s = trim_trailing_whitespace((const char *) p, len);
      ESP_LOGI(TAG, "  %s : (%s)", datapoint_id.name.c_str(), s.c_str());
      this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type, .value_string = s});
      break;
    }
    case EconetDatapointType::ENUM_TEXT: {
      p += 3;
      len -= 3;
      if (len < 2) {
        ESP_LOGE(TAG, "Expected len of at least 2 but was %d for %s", len, datapoint_id.name.c_str());
        return;
      }
      uint8_t item_value = p[0];
      uint8_t item_text_len = p[1];
      if (item_text_len != len - 2) {
        ESP_LOGE(TAG, "Expected text len of %d but was %d for %s", len - 2, item_text_len, datapoint_id.name.c_str());
        return;
      }
      std::string s = trim_trailing_whitespace((const char *) p + 2, item_text_len);
      ESP_LOGI(TAG, "  %s : %d (%s)", datapoint_id.name.c_str(), item_value, s.c_str());
      this->send_datapoint_(datapoint_id,
                            EconetDatapoint{.type = item_type, .value_enum = item_value, .value_string = s});
      break;
    }
    case EconetDatapointType::RAW:
      // Handled separately since it seems it cannot be requested together with other objects.
      break;
    case EconetDatapointType::UNSUPPORTED:
      ESP_LOGW(TAG, "  %s : UNSUPPORTED", datapoint_id.name.c_str());
      this->send_datapoint_(datapoint_id, EconetDatapoint{.type = item_type});
      break;
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
  loop_now_ = now;

  if ((now - this->last_read_data_ > RECEIVE_TIMEOUT) && !rx_message_.empty()) {
    ESP_LOGW(TAG, "Ignoring partially received message due to timeout");
    rx_message_.clear();
    read_req_.awaiting_res = false;
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

  this->make_request_();
}

void Econet::write_value_(const std::string &object, EconetDatapointType type, float value, uint32_t address) {
  if (address == 0) {
    address = dst_adr_;
  }
  std::vector<uint8_t> data;

  data.push_back(1);
  data.push_back(1);
  data.push_back((uint8_t) type);
  data.push_back(1);
  data.push_back(0);
  data.push_back(0);

  for (int j = 0; j < OBJ_NAME_SIZE; j++) {
    data.push_back(j < object.length() ? object[j] : 0);
  }

  uint32_t f_to_32 = float_to_uint32(value);

  data.push_back((uint8_t) (f_to_32 >> 24));
  data.push_back((uint8_t) (f_to_32 >> 16));
  data.push_back((uint8_t) (f_to_32 >> 8));
  data.push_back((uint8_t) (f_to_32));

  transmit_message_(WRITE_COMMAND, data, address);
}

void Econet::request_strings_() {
  std::vector<std::string> objects;
  uint32_t dst_adr = dst_adr_;
  if (!datapoint_ids_for_read_service_.empty()) {
    objects.push_back(datapoint_ids_for_read_service_.front().name);
    dst_adr = datapoint_ids_for_read_service_.front().address;
    datapoint_ids_for_read_service_.pop();
  } else {
    // Impose a longer delay restriction for general periodically requested messages
    if (loop_now_ - last_read_request_ < min_delay_between_read_requests_) {
      return;
    }
    for (auto request_mod = request_mods_.begin(); request_mod != request_mods_.end(); ++request_mod) {
      if ((loop_now_ - request_mod_last_requested_[*request_mod]) >=
          request_mod_update_interval_millis_[*request_mod]) {
        std::copy(request_datapoint_ids_[*request_mod].begin(), request_datapoint_ids_[*request_mod].end(),
                  back_inserter(objects));
        request_mod_last_requested_[*request_mod] = loop_now_;
        dst_adr = request_mod_addresses_[*request_mod];
        break;
      }
    }
  }
  std::vector<std::string>::iterator iter;
  for (iter = objects.begin(); iter != objects.end();) {
    if (request_once_datapoint_ids_.count(EconetDatapointID{.name = *iter, .address = dst_adr}) == 1 &&
        datapoints_.count(EconetDatapointID{.name = *iter, .address = dst_adr}) == 1) {
      iter = objects.erase(iter);

    } else {
      ++iter;
    }
  }
  if (objects.empty()) {
    return;
  }

  last_read_request_ = loop_now_;

  std::vector<uint8_t> data;

  // Read Class
  if (objects.size() == 1 && raw_datapoint_ids_.count(EconetDatapointID{.name = objects[0], .address = dst_adr}) == 1) {
    data.push_back(1);
  } else {
    data.push_back(2);
  }

  // Read Property
  data.push_back(1);

  join_obj_names(objects, &data);

  transmit_message_(READ_COMMAND, data, dst_adr);
}

void Econet::transmit_message_(uint8_t command, const std::vector<uint8_t> &data, uint32_t dst_adr, uint32_t src_adr) {
  if (dst_adr == 0) {
    dst_adr = dst_adr_;
  }
  if (src_adr == 0) {
    src_adr = src_adr_;
  }
  last_request_ = loop_now_;

  tx_message_.clear();

  address_to_bytes(dst_adr, &tx_message_);
  address_to_bytes(src_adr, &tx_message_);

  tx_message_.push_back(data.size());
  tx_message_.push_back(0);
  tx_message_.push_back(0);
  tx_message_.push_back(command);
  tx_message_.insert(tx_message_.end(), data.begin(), data.end());

  uint16_t crc = crc16(&tx_message_[0], tx_message_.size(), 0);
  tx_message_.push_back(crc);
  tx_message_.push_back(crc >> 8);

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
  }

  this->write_array(&tx_message_[0], tx_message_.size());
  this->flush();

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(false);
  }

  parse_tx_message_();
}

void Econet::set_float_datapoint_value(const std::string &datapoint_id, float value, uint32_t address) {
  ESP_LOGD(TAG, "Setting datapoint %s to %f", datapoint_id.c_str(), value);
  this->set_datapoint_(EconetDatapointID{.name = datapoint_id, .address = address},
                       EconetDatapoint{.type = EconetDatapointType::FLOAT, .value_float = value});
}

void Econet::set_enum_datapoint_value(const std::string &datapoint_id, uint8_t value, uint32_t address) {
  ESP_LOGD(TAG, "Setting datapoint %s to %u", datapoint_id.c_str(), value);
  this->set_datapoint_(EconetDatapointID{.name = datapoint_id, .address = address},
                       EconetDatapoint{.type = EconetDatapointType::ENUM_TEXT, .value_enum = value});
}

void Econet::set_datapoint_(EconetDatapointID datapoint_id, const EconetDatapoint &value) {
  if (datapoint_id.address == 0) {
    datapoint_id.address = dst_adr_;
  }
  auto specific = datapoint_id;
  auto any = EconetDatapointID{.name = datapoint_id.name, .address = 0};
  bool send_specific = true;
  bool send_any = true;
  if (datapoints_.count(specific) == 0) {
    ESP_LOGW(TAG, "Setting unknown datapoint %s", datapoint_id.name.c_str());
  } else {
    EconetDatapoint old_value = datapoints_[specific];
    if (old_value.type != value.type) {
      ESP_LOGE(TAG, "Attempt to set datapoint %s with incorrect type", datapoint_id.name.c_str());
      return;
    } else if (old_value == value && pending_writes_.count(specific) == 0) {
      ESP_LOGV(TAG, "Not setting unchanged value for datapoint %s", datapoint_id.name.c_str());
      send_specific = false;
    }
  }
  if (datapoints_.count(any) == 0) {
    ESP_LOGW(TAG, "Setting unknown datapoint %s", datapoint_id.name.c_str());
  } else {
    EconetDatapoint old_value = datapoints_[any];
    if (old_value == value) {
      ESP_LOGV(TAG, "Not setting unchanged value for datapoint %s", datapoint_id.name.c_str());
      send_any = false;
    }
  }
  if (send_specific) {
    pending_writes_[specific] = value;
    send_datapoint_(specific, value);
  } else if (send_any) {
    send_datapoint_(specific, value);
  }
}

void Econet::send_datapoint_(EconetDatapointID datapoint_id, const EconetDatapoint &value) {
  auto specific = EconetDatapointID{.name = datapoint_id.name, .address = datapoint_id.address};
  auto any = EconetDatapointID{.name = datapoint_id.name, .address = 0};
  bool send_specific = true;
  bool send_any = true;
  if (datapoints_.count(specific) == 1) {
    EconetDatapoint old_value = datapoints_[specific];
    if (old_value == value) {
      ESP_LOGV(TAG, "Not sending unchanged value for datapoint %s", specific.datapoint_id.name.c_str());
      send_specific = false;
    }
  }
  if (send_specific) {
    datapoints_[specific] = value;
    for (auto &listener : this->listeners_) {
      if (listener.datapoint_id.name == datapoint_id.name &&
          (listener.datapoint_id.address == 0 || listener.datapoint_id.address == datapoint_id.address)) {
        listener.on_datapoint(value);
      }
    }
  }
  if (datapoints_.count(any) == 1) {
    EconetDatapoint old_value = datapoints_[any];
    if (old_value == value) {
      ESP_LOGV(TAG, "Not sending unchanged value for datapoint %s", any.datapoint_id.name.c_str());
      send_any = false;
    }
  }
  if (send_any) {
    datapoints_[any] = value;
    for (auto &listener : this->listeners_) {
      if (listener.datapoint_id.name == datapoint_id.name &&
          (listener.datapoint_id.address == 0 || listener.datapoint_id.address == datapoint_id.address)) {
        listener.on_datapoint(value);
      }
    }
  }
}

void Econet::register_listener(const std::string &datapoint_id, int8_t request_mod, bool request_once,
                               const std::function<void(EconetDatapoint)> &func, bool is_raw_datapoint,
                               uint32_t src_adr) {
  if (request_mod >= 0 && request_mod < request_datapoint_ids_.size()) {
    request_datapoint_ids_[request_mod].insert(datapoint_id);
    request_mods_.insert(request_mod);
    min_delay_between_read_requests_ = std::max(min_update_interval_millis_ / request_mods_.size(), REQUEST_DELAY);
    if (request_once) {
      request_once_datapoint_ids_.insert(EconetDatapointID{.name = datapoint_id, .address = src_adr});
    }
  }
  if (is_raw_datapoint) {
    raw_datapoint_ids_.insert(EconetDatapointID{.name = datapoint_id, .address = src_adr});
  }
  auto listener = EconetDatapointListener{
      .datapoint_id = EconetDatapointID{.name = datapoint_id, .address = src_adr},
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &kv : this->datapoints_) {
    if (kv.first.name == datapoint_id && (kv.first.address == src_adr || kv.first.address == 0)) {
      func(kv.second);
    }
  }
}

// Called from a Home Assistant exposed service to read a datapoint.
// Fires a Home Assistant event: "esphome.econet_event" with the response.
void Econet::homeassistant_read(std::string datapoint_id, uint32_t address) {
  if (address == 0) {
    address = src_adr_;
  }
  register_listener(datapoint_id, -1, true, [this, datapoint_id](const EconetDatapoint &datapoint) {
    std::map<std::string, std::string> data;
    data["datapoint_id"] = datapoint_id;
    switch (datapoint.type) {
      case EconetDatapointType::FLOAT:
        data["type"] = "FLOAT";
        data["value"] = std::to_string(datapoint.value_float);
        break;
      case EconetDatapointType::ENUM_TEXT:
        data["type"] = "ENUM_TEXT";
        data["value"] = std::to_string(datapoint.value_enum);
        data["value_string"] = datapoint.value_string;
        break;
      case EconetDatapointType::TEXT:
        data["type"] = "TEXT";
        data["value_string"] = datapoint.value_string;
        break;
      case EconetDatapointType::RAW:
        data["type"] = "RAW";
        data["value_raw"] = format_hex_pretty(datapoint.value_raw).c_str();
        break;
      case EconetDatapointType::UNSUPPORTED:
        data["type"] = "UNSUPPORTED";
        break;
    }
    capi_.fire_homeassistant_event("esphome.econet_event", data);
  });
  datapoint_ids_for_read_service_.push(EconetDatapointID{.name = datapoint_id, .address = address});
}
void Econet::homeassistant_write(std::string datapoint_id, uint8_t value) {
  set_datapoint_(EconetDatapointID{.name = datapoint_id, .address = 0},
                 EconetDatapoint{.type = EconetDatapointType::ENUM_TEXT, .value_enum = value});
}

void Econet::homeassistant_write(std::string datapoint_id, float value) {
  set_datapoint_(EconetDatapointID{.name = datapoint_id, .address = 0},
                 EconetDatapoint{.type = EconetDatapointType::FLOAT, .value_float = value});
}

}  // namespace econet
}  // namespace esphome
