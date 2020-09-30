#pragma once

#include "esphome/core/log.h"
#include "ndef_record.h"

static const uint8_t MAX_NDEF_RECORDS = 4;

namespace esphome {
namespace nfc {

class NdefMessage {
 public:
  NdefMessage(){};
  NdefMessage(std::vector<uint8_t> data);

  std::vector<NdefRecord> get_records() { return this->records_; };

  boolean add_record(NdefRecord* record);
  boolean add_text_record(std::string text);
  boolean add_text_record(std::string text, std::string encoding);
  boolean add_uri_record(std::string uri);
  boolean add_empty_record();

  void encode(uint8_t* data);

 protected:
  std::vector<NdefRecord> records_;
};

}  // namespace nfc
}  // namespace esphome
