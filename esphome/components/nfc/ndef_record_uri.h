#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "ndef_record.h"

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
  NdefRecordUri(const std::string &URI) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "U";
    this->URI_ = URI;
  };
  NdefRecordUri(const std::string &URI, const std::string &id) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "U";
    this->URI_ = URI;
    this->id_ = id;
  };
  NdefRecordUri(const NdefRecordUri &rhs) {
    this->tnf_ = rhs.tnf_;
    this->type_ = rhs.type_;
    this->URI_ = rhs.URI_;
    this->id_ = rhs.id_;
  };
  void set_URI(const std::string &URI) { this->URI_ = URI; };

  std::vector<uint8_t> getEncodedPayload();
  const std::string get_payload() { return this->URI_; };
 
 protected:
  std::string URI_;
};

}  // namespace nfc
}  // namespace esphome
