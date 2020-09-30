#include "ndef_message.h"

namespace esphome {
namespace nfc {

static const char *TAG = "nfc.ndef_message";

NdefMessage::NdefMessage(std::vector<uint8_t> &data) {
  uint8_t index = 0;
  while (index <= data.size()) {
    uint8_t tnf_byte = data[index];
    bool me = tnf_byte & 0x40;
    bool sr = tnf_byte & 0x10;
    bool il = tnf_byte & 0x08;
    uint8_t tnf = tnf_byte & 0x07;

    auto record = make_unique<NdefRecord>();
    record->set_tnf(tnf);

    index++;
    uint8_t type_length = data[index];
    uint32_t payload_length = 0;
    if (sr) {
      index++;
      payload_length = data[index];
    } else {
      payload_length = (static_cast<uint32_t>(data[index]) << 24) | (static_cast<uint32_t>(data[index + 1]) << 16) |
                       (static_cast<uint32_t>(data[index + 2]) << 8) | static_cast<uint32_t>(data[index + 3]);
      index += 4;
    }

    uint8_t id_length = 0;
    if (il) {
      index++;
      id_length = data[index];
    }

    index++;
    record->set_type(std::string(data[index], type_length));
    index += type_length;

    if (il) {
      record->set_id(std::string(data[index], id_length));
      index += id_length;
    }

    record->set_payload(std::string(data[index], payload_length));
    index += payload_length;

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
  this->records_.push_back(std::move(record));
  return true;
}

bool NdefMessage::add_text_record(const std::string &text) { return this->add_text_record(text, "en"); };

bool NdefMessage::add_text_record(const std::string &text, const std::string &encoding) {
  std::string payload = to_string(text.length()) + encoding + text;
  auto r = make_unique<NdefRecord>(TNF_WELL_KNOWN, "T", payload);
  return this->add_record(std::move(r));
}

bool NdefMessage::add_uri_record(const std::string &uri) {
  std::string payload = '\0' + uri;
  auto r = make_unique<NdefRecord>(TNF_WELL_KNOWN, "U", payload);
  return this->add_record(std::move(r));
}

bool NdefMessage::add_empty_record() {
  auto r = make_unique<NdefRecord>();
  r->set_tnf(TNF_EMPTY);
  return this->add_record(std::move(r));
}

void NdefMessage::encode(uint8_t *data) {
  uint8_t *data_ptr = &data[0];

  for (uint8_t i = 0; i < this->records_.size(); i++) {
    this->records_[i]->encode(data_ptr, i == 0, (i + 1) == this->records_.size());
    data_ptr += this->records_[i]->get_encoded_size();
  }
}

}  // namespace nfc
}  // namespace esphome
