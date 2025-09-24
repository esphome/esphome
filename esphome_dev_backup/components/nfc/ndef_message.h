#pragma once

#include <memory>
#include <vector>

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
  NdefMessage() = default;
  NdefMessage(std::vector<uint8_t> &data);
  NdefMessage(const NdefMessage &msg) {
    records_.reserve(msg.records_.size());
    for (const auto &r : msg.records_) {
      records_.emplace_back(r->clone());
    }
  }

  const std::vector<std::shared_ptr<NdefRecord>> &get_records() { return this->records_; };

  bool add_record(std::unique_ptr<NdefRecord> record);
  bool add_text_record(const std::string &text);
  bool add_text_record(const std::string &text, const std::string &encoding);
  bool add_uri_record(const std::string &uri);

  std::vector<uint8_t> encode();

 protected:
  std::vector<std::shared_ptr<NdefRecord>> records_;
};

}  // namespace nfc
}  // namespace esphome
