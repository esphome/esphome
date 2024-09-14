#include "esphome/core/helpers.h"
#include "constants.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

SmlFile::SmlFile(bytes buffer) : buffer_(std::move(buffer)) {
  // extract messages
  this->pos_ = 0;
  while (this->pos_ < this->buffer_.size()) {
    if (this->buffer_[this->pos_] == 0x00)
      break;  // EndOfSmlMsg

    SmlNode message = SmlNode();
    if (!this->setup_node(&message))
      break;
    this->messages.emplace_back(message);
  }
}

bool SmlFile::setup_node(SmlNode *node) {
  // If the TL field is 0x00, this is the end of the message
  // (see 6.3.1 of SML protocol definition)
  if (this->buffer_[this->pos_] == 0x00) {
    // Increment past this byte and signal that the message is done
    this->pos_ += 1;
    return true;
  }

  // Extract data from initial TL field
  uint8_t type = (this->buffer_[this->pos_] >> 4) & 0x07;     // type without overlength info
  bool overlength = (this->buffer_[this->pos_] >> 4) & 0x08;  // overlength information
  uint8_t length = this->buffer_[this->pos_] & 0x0f;          // length (including TL bytes)

  // Check if we need additional length bytes
  if (overlength) {
    // Shift the current length to the higher nibble
    // and add the lower nibble of the next byte to the length
    length = (length << 4) + (this->buffer_[this->pos_ + 1] & 0x0f);
    // We are basically done with the first TL field now,
    // so increment past that, we now point to the second TL field
    this->pos_ += 1;
    // Decrement the length for value fields (not lists),
    // since the byte we just handled is counted as part of the field
    // in case of values but not for lists
    if (type != SML_LIST)
      length -= 1;

    // Technically, this is not enough, the standard allows for more than two length fields.
    // However I don't think it is very common to have more than 255 entries in a list
  }

  // We are done with the last TL field(s), so advance the position
  this->pos_ += 1;
  // and decrement the length for non-list fields
  if (type != SML_LIST)
    length -= 1;

  // Check if the buffer length is long enough
  if (this->pos_ + length > this->buffer_.size())
    return false;

  node->type = type;
  node->nodes.clear();
  node->value_bytes.clear();

  if (type == SML_LIST) {
    node->nodes.reserve(length);
    for (size_t i = 0; i != length; i++) {
      SmlNode child_node = SmlNode();
      if (!this->setup_node(&child_node))
        return false;
      node->nodes.emplace_back(child_node);
    }
  } else {
    // Value starts at the current position
    // Value ends "length" bytes later,
    // (since the TL field is counted but already subtracted from length)
    node->value_bytes = bytes(this->buffer_.begin() + this->pos_, this->buffer_.begin() + this->pos_ + length);
    // Increment the pointer past all consumed bytes
    this->pos_ += length;
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

std::string bytes_repr(const bytes &buffer) {
  std::string repr;
  for (auto const value : buffer) {
    repr += str_sprintf("%02x", value & 0xff);
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

  // sign extension for abbreviations of leading ones (e.g. 3 byte transmissions, see 6.2.2 of SML protocol definition)
  // see https://stackoverflow.com/questions/42534749/signed-extension-from-24-bit-to-32-bit-in-c
  if (buffer.size() < 8) {
    const int bits = buffer.size() * 8;
    const uint64_t m = 1ull << (bits - 1);
    tmp = (tmp ^ m) - m;
  }

  val = (int64_t) tmp;
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
  return str_sprintf("%d-%d:%d.%d.%d", this->code[0], this->code[1], this->code[2], this->code[3], this->code[4]);
}

}  // namespace sml
}  // namespace esphome
