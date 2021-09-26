#include <iostream>
#include <sstream>
#include <iomanip>
#include "constants.h"
#include "sml_parser.h"

using namespace std;

namespace esphome {
namespace sml {

uint16_t get_entry_length(const bytes &buffer, unsigned int &pos) {
  uint16_t type = buffer[pos] >> 4;
  uint16_t length = buffer[pos] & 0x0f;

  if (type & 0x08) {  // we have a long list/value (>15 entries)
    length = (length << 4) + (buffer[pos + 1] & 0x0f);
  };
  return length;
}

SmlBase::SmlBase(const bytes &buffer, unsigned int &pos) : buffer_(buffer), startpos_(pos) {
  this->type = buffer[pos] >> 4;
  this->length = get_entry_length(buffer, pos);
}

SmlNode::SmlNode(const bytes &buffer, unsigned int &pos) : SmlBase(buffer, pos) {
  if (this->buffer_[pos] == 0x00)  // end of message
    pos += 1;
  else if (this->is_list()) {  // list
    pos += 1;
    for (unsigned int i = 0; i != this->length; i++) {
      this->nodes.emplace_back(SmlNode(this->buffer_, pos));
    }
  } else {  // value
    this->value_bytes = bytes(this->buffer_.begin() + pos + 1, this->buffer_.begin() + pos + this->length);
    pos += length;
  }
}

bool SmlNode::is_list() { return ((this->type & 0x07) == SML_LIST); }

SmlFile::SmlFile(bytes buffer) : buffer_(std::move(buffer)) {
  // extract messages
  unsigned int pos = 8;
  while (pos < this->buffer_.size() - 8) {
    if (this->buffer_[pos] == 0x00)
      break;  // fill byte detected -> no more messages

    this->messages.emplace_back(SmlNode(this->buffer_, pos));
  }
}

std::vector<ObisInfo> SmlFile::get_obis_info() {
  std::vector<ObisInfo> obis_info;
  for (unsigned int i = 0; i != this->messages.size(); i++) {
    SmlNode message = this->messages[i];
    SmlNode message_body = message.nodes[3];
    uint16_t message_type = bytes_to_uint(message_body.nodes[0].value_bytes);
    if (message_type != SML_GET_LIST_RES)
      continue;

    SmlNode get_list_response = message_body.nodes[1];
    bytes server_id = get_list_response.nodes[1].value_bytes;
    SmlNode val_list = get_list_response.nodes[4];

    std::vector<SmlNode> nodes = val_list.nodes;
    for (unsigned int j = 0; j != nodes.size(); j++) {
      obis_info.emplace_back(ObisInfo(server_id, nodes[j]));
    }
  }
  return obis_info;
}

char check_sml_data(const bytes &buffer) {
  uint16_t crc_received = (buffer.at(buffer.size() - 2) << 8) | buffer.at(buffer.size() - 1);
  if (crc_received == calc_crc16_x25(buffer))
    return CHECK_CRC16_X25_SUCCESS;
  else if (crc_received == calc_crc16_kermit(buffer))
    return CHECK_CRC16_KERMIT_SUCCESS;
  return CHECK_CRC16_FAILED;
}

uint16_t calc_crc16_x25(const bytes &buffer) {
  uint16_t crcsum = 0xffff;
  unsigned int len = buffer.size() - 2;
  unsigned int idx = 0;

  while (len--) {
    crcsum = (crcsum >> 8) ^ CRC16_X25_TABLE[(crcsum & 0xff) ^ buffer.at(idx++)];
  }

  crcsum ^= 0xffff;
  crcsum = (crcsum >> 8) | ((crcsum & 0xff) << 8);
  return crcsum;
}

uint16_t calc_crc16_kermit(const bytes &buffer) {
  uint16_t crcsum = 0x00;
  unsigned int len = buffer.size() - 2;
  unsigned int idx = 0;

  while (len--) {
    crcsum = (crcsum >> 8) ^ CRC16_X25_TABLE[(crcsum & 0xff) ^ buffer.at(idx++)];
  }

  return crcsum;
}

string bytes_repr(const bytes &buffer) {
  ostringstream bytes_stream;
  for (unsigned int i = 0; i != buffer.size(); i++) {
    bytes_stream << setfill('0') << setw(2) << hex << (buffer[i] & 0xff);
  }
  return bytes_stream.str();
}

uint64_t bytes_to_uint(const bytes &buffer) {
  uint64_t val = 0;
  for (unsigned int i = 0; i != buffer.size(); i++) {
    val = (val << 8) + buffer.at(i);
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

string bytes_to_string(const bytes &buffer) { return string(buffer.begin(), buffer.end()); }

ObisInfo::ObisInfo(bytes server_id, SmlNode val_list_entry) {
  this->server_id = move(server_id);
  this->code = val_list_entry.nodes[0].value_bytes;
  this->status = val_list_entry.nodes[1].value_bytes;
  this->unit = bytes_to_uint(val_list_entry.nodes[3].value_bytes);
  this->scaler = bytes_to_int(val_list_entry.nodes[4].value_bytes);
  SmlNode value_node = val_list_entry.nodes[5];
  this->value = value_node.value_bytes;
  this->value_type = value_node.type;
}

string ObisInfo::code_repr() const {
  ostringstream code_stream;
  code_stream << (unsigned int) this->code[0];
  code_stream << "-";
  code_stream << (unsigned int) this->code[1];
  code_stream << ":";
  code_stream << (unsigned int) this->code[2];
  code_stream << ".";
  code_stream << (unsigned int) this->code[3];
  code_stream << ".";
  code_stream << (unsigned int) this->code[4];
  return code_stream.str();
}

}  // namespace sml
}  // namespace esphome
