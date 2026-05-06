#pragma once

#include <memory>
#include <vector>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "ndef_message.h"

namespace esphome {
namespace nfc {

// NFC UIDs are 4, 7, or 10 bytes depending on tag type
static constexpr size_t NFC_UID_MAX_LENGTH = 10;
using NfcTagUid = StaticVector<uint8_t, NFC_UID_MAX_LENGTH>;

class NfcTag {
 public:
  NfcTag() { this->tag_type_ = "Unknown"; };
  NfcTag(const NfcTagUid &uid) {
    this->uid_ = uid;
    this->tag_type_ = "Unknown";
  };
  NfcTag(const NfcTagUid &uid, const std::string &tag_type) {
    this->uid_ = uid;
    this->tag_type_ = tag_type;
  };
  NfcTag(const NfcTagUid &uid, const std::string &tag_type, std::unique_ptr<nfc::NdefMessage> ndef_message) {
    this->uid_ = uid;
    this->tag_type_ = tag_type;
    this->ndef_message_ = std::move(ndef_message);
  };
  NfcTag(const NfcTagUid &uid, const std::string &tag_type, std::vector<uint8_t> &ndef_data) {
    this->uid_ = uid;
    this->tag_type_ = tag_type;
    this->ndef_message_ = make_unique<NdefMessage>(ndef_data);
  };
  NfcTag(const NfcTag &rhs) {
    uid_ = rhs.uid_;
    tag_type_ = rhs.tag_type_;
    if (rhs.ndef_message_ != nullptr)
      ndef_message_ = make_unique<NdefMessage>(*rhs.ndef_message_);
  }

  NfcTagUid &get_uid() { return this->uid_; };
  const std::string &get_tag_type() { return this->tag_type_; };
  bool has_ndef_message() { return this->ndef_message_ != nullptr; };
  const std::shared_ptr<NdefMessage> &get_ndef_message() { return this->ndef_message_; };
  void set_ndef_message(std::unique_ptr<NdefMessage> ndef_message) { this->ndef_message_ = std::move(ndef_message); };

 protected:
  NfcTagUid uid_;
  std::string tag_type_;
  std::shared_ptr<NdefMessage> ndef_message_;
};

}  // namespace nfc
}  // namespace esphome
