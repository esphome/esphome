#pragma once

#include "esphome/core/log.h"
#include "ndef_message.h"

namespace esphome {
namespace nfc {

class NfcTag {
 public:
  NfcTag() {
    this->uid_ = {};
    this->tag_type_ = "Unknown";
  };
  NfcTag(std::vector<uint8_t> &uid) {
    this->uid_ = uid;
    this->tag_type_ = "Unknown";
  };
  NfcTag(std::vector<uint8_t> &uid, const std::string &tag_type) {
    this->uid_ = uid;
    this->tag_type_ = tag_type;
  };
  NfcTag(std::vector<uint8_t> &uid, const std::string &tag_type, nfc::NdefMessage *ndef_message) {
    this->uid_ = uid;
    this->tag_type_ = tag_type;
    this->ndef_message_ = ndef_message;
  };
  NfcTag(std::vector<uint8_t> &uid, const std::string &tag_type, std::vector<uint8_t> &ndef_data) {
    this->uid_ = uid;
    this->tag_type_ = tag_type;
    this->ndef_message_ = new NdefMessage(ndef_data);
  };

  std::vector<uint8_t> &get_uid() { return this->uid_; };
  const std::string &get_tag_type() { return this->tag_type_; };
  bool has_ndef_message() { return this->ndef_message_ != nullptr; };
  NdefMessage *get_ndef_message() { return this->ndef_message_; };
  void set_ndef_message(NdefMessage *ndef_message) { this->ndef_message_ = ndef_message; };

 protected:
  std::vector<uint8_t> uid_;
  std::string tag_type_;
  NdefMessage *ndef_message_{nullptr};
};

}  // namespace nfc
}  // namespace esphome
