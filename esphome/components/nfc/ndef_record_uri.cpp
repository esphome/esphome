#include "ndef_record_uri.h"
#include "esphome/core/progmem.h"

namespace esphome::nfc {

ESPHOME_LOG_TAG(TAG, "nfc.ndef_record_uri");

// URI identifier codes from the NFC Forum URI record type definition
PROGMEM_STRING_TABLE(UriPrefixStrings, "", "http://www.", "https://www.", "http://", "https://",
                     "tel:", "mailto:", "ftp://anonymous:anonymous@", "ftp://ftp.", "ftps://", "sftp://", "smb://",
                     "nfs://", "ftp://", "dav://", "news:", "telnet://", "imap:", "rtsp://",
                     "urn:", "pop:", "sip:", "sips:", "tftp:", "btspp://", "btl2cap://", "btgoep://", "tcpobex://",
                     "irdaobex://", "file://",
                     "urn:epc:id:", "urn:epc:tag:", "urn:epc:pat:", "urn:epc:raw:", "urn:epc:", "urn:nfc:");

NdefRecordUri::NdefRecordUri(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    ESP_LOGE(TAG, "Record payload too short");
    return;
  }

  uint8_t payload_identifier = payload[0];  // First byte of payload is prefix code

  std::string uri(payload.begin() + 1, payload.end());

  if (payload_identifier > 0x00 && payload_identifier <= UriPrefixStrings::LAST_INDEX) {
    char prefix[UriPrefixStrings::MAX_LENGTH + 1];
    ESPHOME_strncpy_P(prefix, reinterpret_cast<ESPHOME_PGM_P>(UriPrefixStrings::get_progmem_str(payload_identifier, 0)),
                      sizeof(prefix));
    uri.insert(0, prefix);
  }

  this->tnf_ = TNF_WELL_KNOWN;
  this->type_ = "U";
  this->set_uri(uri);
}

std::vector<uint8_t> NdefRecordUri::get_encoded_payload() {
  std::vector<uint8_t> data;

  uint8_t payload_prefix = 0x00;
  uint8_t payload_prefix_length = 0x00;
  for (uint8_t i = 1; i < UriPrefixStrings::LAST_INDEX; i++) {
    const auto *prefix = reinterpret_cast<ESPHOME_PGM_P>(UriPrefixStrings::get_progmem_str(i, 0));
    const size_t length = ESPHOME_strlen_P(prefix);
    if (this->uri_.size() >= length && ESPHOME_strncmp_P(this->uri_.c_str(), prefix, length) == 0) {
      payload_prefix = i;
      payload_prefix_length = length;
      break;
    }
  }

  data.push_back(payload_prefix);

  data.insert(data.end(), this->uri_.begin() + payload_prefix_length, this->uri_.end());
  return data;
}

}  // namespace esphome::nfc
