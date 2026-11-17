#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "ndef_record.h"

#include <vector>

namespace esphome::nfc {

class NdefRecordUri : public NdefRecord {
 public:
  NdefRecordUri() {};
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

}  // namespace esphome::nfc
