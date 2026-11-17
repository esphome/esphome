#pragma once

#include <memory>
#include <span>
#include <vector>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "ndef_record.h"
#include "ndef_record_text.h"
#include "ndef_record_uri.h"

namespace esphome::nfc {

static constexpr uint8_t MAX_NDEF_RECORDS = 4;
/// The records of one message, owned in place so parsing a message allocates only the records themselves
using NdefRecords = StaticVector<std::unique_ptr<NdefRecord>, MAX_NDEF_RECORDS>;

class NdefMessage {
 public:
  NdefMessage() = default;
  NdefMessage(std::span<const uint8_t> data);
  NdefMessage(std::vector<uint8_t> &data) : NdefMessage(std::span<const uint8_t>(data)) {}
  NdefMessage(const NdefMessage &msg) {
    for (const auto &r : msg.records_) {
      records_.emplace_next() = r->clone();
    }
  }

  const NdefRecords &get_records() const { return this->records_; };

  bool add_record(std::unique_ptr<NdefRecord> record);
  bool add_text_record(const std::string &text);
  bool add_text_record(const std::string &text, const std::string &encoding);
  bool add_uri_record(const std::string &uri);

  std::vector<uint8_t> encode();

 protected:
  NdefRecords records_;
};

}  // namespace esphome::nfc
