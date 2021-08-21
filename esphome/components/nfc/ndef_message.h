#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "ndef_record.h"
#include "ndef_record_text.h"
#include "ndef_record_uri.h"

namespace esphome {
namespace nfc {

static const uint8_t MAX_NDEF_RECORDS = 4;

class NdefMessage {
 public:
  NdefMessage(){};
  NdefMessage(std::vector<uint8_t> &data);

  std::vector<std::shared_ptr<NdefRecord>> get_records() { return this->records_; };

  bool add_record(std::shared_ptr<NdefRecord> record);
  bool add_text_record(const std::string &text);
  bool add_text_record(const std::string &text, const std::string &encoding);
  bool add_uri_record(const std::string &uri);

  std::vector<uint8_t> encode();

 protected:
  std::vector<std::shared_ptr<NdefRecord>> records_;
};

}  // namespace nfc
}  // namespace esphome
