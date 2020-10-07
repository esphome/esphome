#include "ndef_record.h"

namespace esphome {
namespace nfc {

static const char* TAG = "nfc.ndef_record";

uint32_t NdefRecord::get_encoded_size() {
  uint32_t size = 2;
  if (this->payload_.length() > 255) {
    size += 4;
  } else {
    size += 1;
  }
  if (this->id_.length()) {
    size += 1;
  }
  size += (this->type_.length() + this->payload_.length() + this->id_.length());
  return size;
}

std::vector<uint8_t> NdefRecord::encode(bool first, bool last) {
  std::vector<uint8_t> data;

  data.push_back(get_tnf_byte(first, last));

  data.push_back(this->type_.length());

  if (this->payload_.length() <= 255) {
    data.push_back(this->payload_.length());
  } else {
    data.push_back(0);
    data.push_back(0);
    data.push_back((this->payload_.length() >> 8) & 0xFF);
    data.push_back(this->payload_.length() & 0xFF);
  }

  if (this->id_.length()) {
    data.push_back(this->id_.length());
  }

  data.insert(data.end(), this->type_.begin(), this->type_.end());

  if (this->id_.length()) {
    data.insert(data.end(), this->id_.begin(), this->id_.end());
  }

  data.insert(data.end(), this->payload_.begin(), this->payload_.end());
  return data;
}

uint8_t NdefRecord::get_tnf_byte(bool first, bool last) {
  uint8_t value = this->tnf_;
  if (first) {
    value = value | 0x80;
  }
  if (last) {
    value = value | 0x40;
  }
  if (this->payload_.length() <= 255) {
    value = value | 0x10;
  }
  if (this->id_.length()) {
    value = value | 0x08;
  }
  return value;
};

}  // namespace nfc
}  // namespace esphome
