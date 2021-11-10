#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "ndef_record.h"

namespace esphome {
namespace nfc {

class NdefRecordText : public NdefRecord {
 public:
  NdefRecordText(){};
  NdefRecordText(const std::vector<uint8_t> &payload);
  NdefRecordText(const std::string &language_code, const std::string &text) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "T";
    this->language_code_ = language_code;
    this->text_ = text;
  };
  NdefRecordText(const std::string &language_code, const std::string &text, const std::string &id) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "T";
    this->language_code_ = language_code;
    this->text_ = text;
    this->id_ = id;
  };
  NdefRecordText(const NdefRecordText &) = default;

  std::unique_ptr<NdefRecord> clone() const override { return make_unique<NdefRecordText>(*this); };

  std::vector<uint8_t> get_encoded_payload() override;

  const std::string &get_payload() const override { return this->text_; };

 protected:
  std::string text_;
  std::string language_code_;
};

}  // namespace nfc
}  // namespace esphome
