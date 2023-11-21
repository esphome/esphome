#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "ndef_record.h"

#include <vector>

namespace esphome {
namespace nfc {

static const uint8_t PAYLOAD_IDENTIFIERS_COUNT = 0x23;
static const char *const PAYLOAD_IDENTIFIERS[] = {"",
                                                  "http://www.",
                                                  "https://www.",
                                                  "http://",
                                                  "https://",
                                                  "tel:",
                                                  "mailto:",
                                                  "ftp://anonymous:anonymous@",
                                                  "ftp://ftp.",
                                                  "ftps://",
                                                  "sftp://",
                                                  "smb://",
                                                  "nfs://",
                                                  "ftp://",
                                                  "dav://",
                                                  "news:",
                                                  "telnet://",
                                                  "imap:",
                                                  "rtsp://",
                                                  "urn:",
                                                  "pop:",
                                                  "sip:",
                                                  "sips:",
                                                  "tftp:",
                                                  "btspp://",
                                                  "btl2cap://",
                                                  "btgoep://",
                                                  "tcpobex://",
                                                  "irdaobex://",
                                                  "file://",
                                                  "urn:epc:id:",
                                                  "urn:epc:tag:",
                                                  "urn:epc:pat:",
                                                  "urn:epc:raw:",
                                                  "urn:epc:",
                                                  "urn:nfc:"};

class NdefRecordUri : public NdefRecord {
 public:
  NdefRecordUri(){};
  NdefRecordUri(const std::vector<uint8_t> &payload);
  NdefRecordUri(const std::string &uri) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "U";
    this->uri_ = uri;
  };
  NdefRecordUri(const std::string &uri, const std::string &id) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "U";
    this->uri_ = uri;
    this->id_ = id;
  };
  NdefRecordUri(const NdefRecordUri &) = default;
  std::unique_ptr<NdefRecord> clone() const override { return make_unique<NdefRecordUri>(*this); };

  void set_uri(const std::string &uri) { this->uri_ = uri; };

  std::vector<uint8_t> get_encoded_payload() override;
  const std::string &get_payload() const override { return this->uri_; };

 protected:
  std::string uri_;
};

}  // namespace nfc
}  // namespace esphome
