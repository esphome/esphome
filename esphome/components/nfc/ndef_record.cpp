#include "ndef_record.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_record";

NdefRecord::NdefRecord(std::vector<uint8_t> payload_data) {
  this->payload_ = std::string(payload_data.begin(), payload_data.end());
}

std::vector<uint8_t> NdefRecord::encode(bool first, bool last) {
  std::vector<uint8_t> data;

  // Get encoded payload, this is overridden by more specific record classes
  std::vector<uint8_t> payload_data = get_encoded_payload();

  size_t payload_length = payload_data.size();

  data.push_back(this->create_flag_byte(first, last, payload_length));

  data.push_back(this->type_.length());

  if (payload_length <= 255) {
    data.push_back(payload_length);
  } else {
    data.push_back(0);
    data.push_back(0);
    data.push_back((payload_length >> 8) & 0xFF);
    data.push_back(payload_length & 0xFF);
  }

  if (!this->id_.empty()) {
    data.push_back(this->id_.length());
  }

  data.insert(data.end(), this->type_.begin(), this->type_.end());

  if (!this->id_.empty()) {
    data.insert(data.end(), this->id_.begin(), this->id_.end());
  }

  data.insert(data.end(), payload_data.begin(), payload_data.end());
  return data;
}

uint8_t NdefRecord::create_flag_byte(bool first, bool last, size_t payload_size) {
  uint8_t value = this->tnf_ & 0b00000111;
  if (first) {
    value = value | 0x80;  // Set MB bit
  }
  if (last) {
    value = value | 0x40;  // Set ME bit
  }
  if (payload_size <= 255) {
    value = value | 0x10;  // Set SR bit
  }
  if (!this->id_.empty()) {
    value = value | 0x08;  // Set IL bit
  }
  return value;
};

}  // namespace nfc
}  // namespace esphome
