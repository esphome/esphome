#include "nfc_helpers.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.helpers";

bool has_ha_tag_ndef(NfcTag &tag) { return !get_ha_tag_ndef(tag).empty(); }

std::string get_ha_tag_ndef(NfcTag &tag) {
  if (!tag.has_ndef_message()) {
    return std::string();
  }
  auto message = tag.get_ndef_message();
  auto records = message->get_records();
  for (const auto &record : records) {
    std::string payload = record->get_payload();
    size_t pos = payload.find(HA_TAG_ID_PREFIX);
    if (pos != std::string::npos) {
      return payload.substr(pos + sizeof(HA_TAG_ID_PREFIX) - 1);
    }
  }
  return std::string();
}

std::string get_random_ha_tag_ndef() {
  static const char ALPHANUM[] = "0123456789abcdef";
  std::string uri = HA_TAG_ID_PREFIX;
  for (int i = 0; i < 8; i++) {
    uri += ALPHANUM[random_uint32() % (sizeof(ALPHANUM) - 1)];
  }
  uri += "-";
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 4; i++) {
      uri += ALPHANUM[random_uint32() % (sizeof(ALPHANUM) - 1)];
    }
    uri += "-";
  }
  for (int i = 0; i < 12; i++) {
    uri += ALPHANUM[random_uint32() % (sizeof(ALPHANUM) - 1)];
  }
  ESP_LOGD("pn7160", "Payload to be written: %s", uri.c_str());
  return uri;
}

}  // namespace nfc
}  // namespace esphome
