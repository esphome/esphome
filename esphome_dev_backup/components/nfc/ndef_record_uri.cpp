#include "ndef_record_uri.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_record_uri";

NdefRecordUri::NdefRecordUri(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    ESP_LOGE(TAG, "Record payload too short");
    return;
  }

  uint8_t payload_identifier = payload[0];  // First byte of payload is prefix code

  std::string uri(payload.begin() + 1, payload.end());

  if (payload_identifier > 0x00 && payload_identifier <= PAYLOAD_IDENTIFIERS_COUNT) {
    uri.insert(0, PAYLOAD_IDENTIFIERS[payload_identifier]);
  }

  this->tnf_ = TNF_WELL_KNOWN;
  this->type_ = "U";
  this->set_uri(uri);
}

std::vector<uint8_t> NdefRecordUri::get_encoded_payload() {
  std::vector<uint8_t> data;

  uint8_t payload_prefix = 0x00;
  uint8_t payload_prefix_length = 0x00;
  for (uint8_t i = 1; i < PAYLOAD_IDENTIFIERS_COUNT; i++) {
    std::string prefix = PAYLOAD_IDENTIFIERS[i];
    if (this->uri_.substr(0, prefix.length()).find(prefix) != std::string::npos) {
      payload_prefix = i;
      payload_prefix_length = prefix.length();
      break;
    }
  }

  data.push_back(payload_prefix);

  data.insert(data.end(), this->uri_.begin() + payload_prefix_length, this->uri_.end());
  return data;
}

}  // namespace nfc
}  // namespace esphome
