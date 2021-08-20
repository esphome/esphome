#include "ndef_record_text.h"
#include "ndef_record.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_record_text";

NdefRecordText::NdefRecordText(const std::vector<uint8_t> &payload)
{

  uint8_t langCodeLength = payload[0] & 0b00111111; //Todo, make use of encoding bit?

  this->langCode_ = std::string(payload.begin() + 1, payload.begin() + 1 + langCodeLength);

  this->text_ = std::string(payload.begin() + 1 + langCodeLength, payload.end());

  ESP_LOGD(TAG,"langCodeLength = %d langCode = %s text = %s",langCodeLength,this->langCode_.c_str(),this->text_.c_str());

  this->tnf_ = TNF_WELL_KNOWN;

  this->type_ = "T";

}

std::vector<uint8_t> NdefRecordText::getEncodedPayload() {
  std::vector<uint8_t> data;

  uint8_t flagByte = this->langCode_.length() & 0b00111111; //UTF8 assumed

  data.push_back(flagByte);

  data.insert(data.end(), this->langCode_.begin(), this->langCode_.end());

  data.insert(data.end(), this->text_.begin(), this->text_.end());
  return data;
}


}  // namespace nfc
}  // namespace esphome
