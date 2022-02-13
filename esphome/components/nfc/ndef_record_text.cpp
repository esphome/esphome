#include "ndef_record_text.h"
#include "ndef_record.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_record_text";

NdefRecordText::NdefRecordText(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    ESP_LOGE(TAG, "Record payload too short");
    return;
  }

  uint8_t language_code_length = payload[0] & 0b00111111;  // Todo, make use of encoding bit?

  this->language_code_ = std::string(payload.begin() + 1, payload.begin() + 1 + language_code_length);

  this->text_ = std::string(payload.begin() + 1 + language_code_length, payload.end());

  this->tnf_ = TNF_WELL_KNOWN;

  this->type_ = "T";
}

std::vector<uint8_t> NdefRecordText::get_encoded_payload() {
  std::vector<uint8_t> data;

  uint8_t flag_byte = this->language_code_.length() & 0b00111111;  // UTF8 assumed

  data.push_back(flag_byte);

  data.insert(data.end(), this->language_code_.begin(), this->language_code_.end());

  data.insert(data.end(), this->text_.begin(), this->text_.end());
  return data;
}

}  // namespace nfc
}  // namespace esphome
