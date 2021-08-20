#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "ndef_record.h"

namespace esphome {
namespace nfc {

class NdefRecordText : public NdefRecord{
 public:
  NdefRecordText(){};
  NdefRecordText(const std::vector<uint8_t> &payload);
  NdefRecordText(const std::string &langCode, const std::string &text) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "T";
    this->langCode_ = langCode;
    this->text_ = text;
  };
  NdefRecordText(const std::string &langCode, const std::string &text, const std::string &id) {
    this->tnf_ = TNF_WELL_KNOWN;
    this->type_ = "T";
    this->langCode_ = langCode;
    this->text_ = text;
    this->id_ = id;
  };
  NdefRecordText(const NdefRecordText &rhs) {
    this->tnf_ = rhs.tnf_;
    this->type_ = rhs.type_;
    this->text_ = rhs.text_;
    this->langCode_ = rhs.langCode_;
    this->id_ = rhs.id_;
  };

  std::vector<uint8_t> getEncodedPayload();

  const std::string get_payload() { return this->text_; };

 private:
  std::string text_;
  std::string langCode_;
};

}  // namespace nfc
}  // namespace esphome
