#include "ndef_record.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_record";

NdefRecord::NdefRecord(std::vector<uint8_t> payloadData)
{
  this->payload_ = std::string(payloadData.begin(),payloadData.end());
}

//Get more specific 
std::vector<uint8_t> NdefRecord::encode(bool first, bool last) {
  std::vector<uint8_t> data;

  data.push_back(this->create_flag_byte(first, last));

  data.push_back(this->type_.length());


  std::vector<uint8_t> payloadData = getEncodedPayload();
  uint32_t payload_length = payloadData.size();

  if (payload_length <= 255) {
    data.push_back(payload_length);
  } else {
    data.push_back(0);
    data.push_back(0);
    data.push_back((payload_length >> 8) & 0xFF);
    data.push_back(payload_length & 0xFF);
  }

  if (this->id_.length()) {
    data.push_back(this->id_.length());
  }

  data.insert(data.end(), this->type_.begin(), this->type_.end());

  if (this->id_.length()) {
    data.insert(data.end(), this->id_.begin(), this->id_.end());
  }

  data.insert(data.end(), payloadData.begin(), payloadData.end());
  return data;
}

uint8_t NdefRecord::create_flag_byte(bool first, bool last) {
  uint8_t value = this->tnf_ && 0b00000111;
  if (first) {
    value = value | 0x80;
  }
  if (last) {
    value = value | 0x40;
  }
  if (this->payload_.size() <= 255) {
    value = value | 0x10;
  }
  if (this->id_.length()) {
    value = value | 0x08;
  }
  return value;
};

}  // namespace nfc
}  // namespace esphome
