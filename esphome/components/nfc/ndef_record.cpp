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

void NdefRecord::encode(uint8_t* data, bool first, bool last) {
  uint8_t* data_ptr = &data[0];

  *data_ptr = get_tnf_byte(first, last);
  data_ptr += 1;

  *data_ptr = this->type_.length();
  data_ptr += 1;

  if (this->payload_.length() <= 255) {
    *data_ptr = this->payload_.length();
    data_ptr += 1;
  } else {
    data_ptr[0] = 0;
    data_ptr[1] = 0;
    data_ptr[2] = (this->payload_.length() >> 8) & 0xFF;
    data_ptr[3] = this->payload_.length() & 0xFF;
    data_ptr += 4;
  }

  if (this->id_.length()) {
    *data_ptr = this->id_.length();
    data_ptr += 1;
  }

  memcpy(data_ptr, this->type_.c_str(), this->type_.length());
  data_ptr += this->type_.length();

  if (this->id_.length()) {
    memcpy(data_ptr, this->id_.c_str(), this->id_.length());
    data_ptr += this->id_.length();
  }

  memcpy(data_ptr, this->payload_.c_str(), this->payload_.length());
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
