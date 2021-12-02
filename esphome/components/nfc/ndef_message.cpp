#include "ndef_message.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_message";

NdefMessage::NdefMessage(std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Building NdefMessage with %zu bytes", data.size());
  uint8_t index = 0;
  while (index <= data.size()) {
    uint8_t tnf_byte = data[index++];
    bool me = tnf_byte & 0x40;      // Message End bit (is set if this is the last record of the message)
    bool sr = tnf_byte & 0x10;      // Short record bit (is set if payload size is less or equal to 255 bytes)
    bool il = tnf_byte & 0x08;      // ID length bit (is set if ID Length field exists)
    uint8_t tnf = tnf_byte & 0x07;  // Type Name Format

    ESP_LOGVV(TAG, "me=%s, sr=%s, il=%s, tnf=%d", YESNO(me), YESNO(sr), YESNO(il), tnf);

    uint8_t type_length = data[index++];
    uint32_t payload_length = 0;
    if (sr) {
      payload_length = data[index++];
    } else {
      payload_length = (static_cast<uint32_t>(data[index]) << 24) | (static_cast<uint32_t>(data[index + 1]) << 16) |
                       (static_cast<uint32_t>(data[index + 2]) << 8) | static_cast<uint32_t>(data[index + 3]);
      index += 4;
    }

    uint8_t id_length = 0;
    if (il) {
      id_length = data[index++];
    }

    ESP_LOGVV(TAG, "Lengths: type=%d, payload=%d, id=%d", type_length, payload_length, id_length);

    std::string type_str(data.begin() + index, data.begin() + index + type_length);

    index += type_length;

    std::string id_str = "";
    if (il) {
      id_str = std::string(data.begin() + index, data.begin() + index + id_length);
      index += id_length;
    }

    std::vector<uint8_t> payload_data(data.begin() + index, data.begin() + index + payload_length);

    std::unique_ptr<NdefRecord> record;

    // Based on tnf and type, create a more specific NdefRecord object
    // constructed from the payload data
    if (tnf == TNF_WELL_KNOWN && type_str == "U") {
      record = make_unique<NdefRecordUri>(payload_data);
    } else if (tnf == TNF_WELL_KNOWN && type_str == "T") {
      record = make_unique<NdefRecordText>(payload_data);
    } else {
      // Could not recognize the record, so store as generic one.
      record = make_unique<NdefRecord>(payload_data);
      record->set_tnf(tnf);
      record->set_type(type_str);
    }

    record->set_id(id_str);

    index += payload_length;

    ESP_LOGV(TAG, "Adding record type %s = %s", record->get_type().c_str(), record->get_payload().c_str());
    this->add_record(std::move(record));

    if (me)
      break;
  }
}

bool NdefMessage::add_record(std::unique_ptr<NdefRecord> record) {
  if (this->records_.size() >= MAX_NDEF_RECORDS) {
    ESP_LOGE(TAG, "Too many records. Max: %d", MAX_NDEF_RECORDS);
    return false;
  }
  this->records_.emplace_back(std::move(record));
  return true;
}

bool NdefMessage::add_text_record(const std::string &text) { return this->add_text_record(text, "en"); };

bool NdefMessage::add_text_record(const std::string &text, const std::string &encoding) {
  return this->add_record(make_unique<NdefRecordText>(encoding, text));
}

bool NdefMessage::add_uri_record(const std::string &uri) { return this->add_record(make_unique<NdefRecordUri>(uri)); }

std::vector<uint8_t> NdefMessage::encode() {
  std::vector<uint8_t> data;

  for (size_t i = 0; i < this->records_.size(); i++) {
    auto encoded_record = this->records_[i]->encode(i == 0, (i + 1) == this->records_.size());
    data.insert(data.end(), encoded_record.begin(), encoded_record.end());
  }
  return data;
}

}  // namespace nfc
}  // namespace esphome
