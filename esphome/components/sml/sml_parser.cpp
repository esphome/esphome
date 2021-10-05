#include "constants.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

SmlFile::SmlFile(bytes buffer) : buffer_(std::move(buffer)) {
  // extract messages
  this->pos_ = 8;
  while (this->pos_ + 8 < this->buffer_.size()) {
    if (this->buffer_[this->pos_] == 0x00)
      break;  // fill byte detected -> no more messages

    SmlNode message = SmlNode();
    if (!this->setup_node(&message))
      break;
    this->messages.emplace_back(message);
  }
}

bool SmlFile::setup_node(SmlNode *node) {
  uint8_t type = this->buffer_[this->pos_] >> 4;      // type including overlength info
  uint8_t length = this->buffer_[this->pos_] & 0x0f;  // length including TL bytes
  bool is_list = (type & 0x07) == SML_LIST;
  bool has_extended_length = type & 0x08;  // we have a long list/value (>15 entries)
  uint8_t parse_length = length;
  if (has_extended_length) {
    length = (length << 4) + (this->buffer_[this->pos_ + 1] & 0x0f);
    parse_length = length - 1;
    this->pos_ += 1;
  }

  if (this->pos_ + parse_length + 8 >= this->buffer_.size())
    return false;

  node->type = type & 0x07;
  node->nodes.clear();
  node->value_bytes.clear();
  if (this->buffer_[this->pos_] == 0x00)  // end of message
    this->pos_ += 1;
  else if (is_list) {  // list
    this->pos_ += 1;
    node->nodes.reserve(parse_length);
    for (size_t i = 0; i != parse_length; i++) {
      SmlNode child_node = SmlNode();
      if (!this->setup_node(&child_node))
        return false;
      node->nodes.emplace_back(child_node);
    }
  } else {  // value
    node->value_bytes =
        bytes(this->buffer_.begin() + this->pos_ + 1, this->buffer_.begin() + this->pos_ + parse_length);
    this->pos_ += parse_length;
  }
  return true;
}

std::vector<ObisInfo> SmlFile::get_obis_info() {
  std::vector<ObisInfo> obis_info;
  for (auto const &message : messages) {
    SmlNode message_body = message.nodes[3];
    uint16_t message_type = bytes_to_uint(message_body.nodes[0].value_bytes);
    if (message_type != SML_GET_LIST_RES)
      continue;

    SmlNode get_list_response = message_body.nodes[1];
    bytes server_id = get_list_response.nodes[1].value_bytes;
    SmlNode val_list = get_list_response.nodes[4];

    for (auto const &val_list_entry : val_list.nodes) {
      obis_info.emplace_back(server_id, val_list_entry);
    }
  }
  return obis_info;
}

char check_sml_data(const bytes &buffer) {
  if (buffer.size() < 2)
    return CHECK_CRC16_FAILED;

  uint16_t crc_received = (buffer.at(buffer.size() - 2) << 8) | buffer.at(buffer.size() - 1);
  if (crc_received == calc_crc16_x25(buffer.data(), buffer.size() - 2))
    return CHECK_CRC16_X25_SUCCESS;
  else if (crc_received == calc_crc16_kermit(buffer.data(), buffer.size() - 2))
    return CHECK_CRC16_KERMIT_SUCCESS;
  return CHECK_CRC16_FAILED;
}

uint16_t calc_crc16_x25(const uint8_t *buffer, size_t length) {
  uint16_t crcsum = 0xffff;
  for (size_t i = 0; i < length; i++)
    crcsum = (crcsum >> 8) ^ CRC16_X25_TABLE[(crcsum & 0xff) ^ buffer[i]];
  crcsum ^= 0xffff;
  crcsum = (crcsum >> 8) | ((crcsum & 0xff) << 8);
  return crcsum;
}

uint16_t calc_crc16_kermit(const uint8_t *buffer, size_t length) {
  uint16_t crcsum = 0x00;
  for (size_t i = 0; i < length; i++)
    crcsum = (crcsum >> 8) ^ CRC16_X25_TABLE[(crcsum & 0xff) ^ buffer[i]];
  return crcsum;
}

std::string bytes_repr(const bytes &buffer) {
  std::string repr;
  for (auto const value : buffer) {
    char buf[3];
    sprintf(buf, "%02x", value & 0xff);
    repr += buf;
  }
  return repr;
}

uint64_t bytes_to_uint(const bytes &buffer) {
  uint64_t val = 0;
  for (auto const value : buffer) {
    val = (val << 8) + value;
  }
  return val;
}

int64_t bytes_to_int(const bytes &buffer) {
  uint64_t tmp = bytes_to_uint(buffer);
  int64_t val;

  switch (buffer.size()) {
    case 1:  // int8
      val = (int8_t) tmp;
      break;
    case 2:  // int16
      val = (int16_t) tmp;
      break;
    case 4:  // int32
      val = (int32_t) tmp;
      break;
    default:  // int64
      val = (int64_t) tmp;
  }
  return val;
}

std::string bytes_to_string(const bytes &buffer) { return std::string(buffer.begin(), buffer.end()); }

ObisInfo::ObisInfo(bytes server_id, SmlNode val_list_entry) : server_id(std::move(server_id)) {
  this->code = val_list_entry.nodes[0].value_bytes;
  this->status = val_list_entry.nodes[1].value_bytes;
  this->unit = bytes_to_uint(val_list_entry.nodes[3].value_bytes);
  this->scaler = bytes_to_int(val_list_entry.nodes[4].value_bytes);
  SmlNode value_node = val_list_entry.nodes[5];
  this->value = value_node.value_bytes;
  this->value_type = value_node.type;
}

std::string ObisInfo::code_repr() const {
  char repr[20];
  sprintf(repr, "%d-%d:%d.%d.%d", this->code[0], this->code[1], this->code[2], this->code[3], this->code[4]);
  return std::string(repr);
}

}  // namespace sml
}  // namespace esphome
