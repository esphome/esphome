#include "ndef_message.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.ndef_message";

NdefMessage::NdefMessage(std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Building NdefMessage with %zu bytes", data.size());
  uint8_t index = 0;
  while (index <= data.size()) {
    uint8_t tnf_byte = data[index++];
    bool me = tnf_byte & 0x40;
    bool sr = tnf_byte & 0x10;
    bool il = tnf_byte & 0x08;
    uint8_t tnf = tnf_byte & 0x07;

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

    std::vector<uint8_t> payloadData(data.begin() + index,data.begin() + index + payload_length);

    std::shared_ptr<NdefRecord> record;

    if(tnf == TNF_WELL_KNOWN && type_str == "U")
    {
      record = std::make_shared<NdefRecordUri>(payloadData);
    }
    else if (tnf == TNF_WELL_KNOWN && type_str == "T")
    {
      record = std::make_shared<NdefRecordText>(payloadData);
    }

    if(record == nullptr) //Could not recognize the record, so store as generic one.
    {
      record = std::make_shared<NdefRecord>(payloadData);
      record->set_tnf(tnf);
      record->set_type(type_str);
    }

    record->set_id(id_str);

    index += payload_length;

    
    ESP_LOGD(TAG, "Adding record type %s = %s", record->get_type().c_str(), record->get_payload().c_str());
    this->add_record(record);
    if (me)
      break;
  }
}

bool NdefMessage::add_record(std::shared_ptr<NdefRecord> record) {
  if (this->records_.size() >= MAX_NDEF_RECORDS) {
    ESP_LOGE(TAG, "Too many records. Max: %d", MAX_NDEF_RECORDS);
    return false;
  }
  this->records_.push_back(record);
  return true;
}

bool NdefMessage::add_text_record(const std::string &text) { return this->add_text_record(text, "en"); };

bool NdefMessage::add_text_record(const std::string &text, const std::string &encoding) {
  auto r = std::make_shared<NdefRecordText>(encoding ,text);
  return this->add_record(r);
}

bool NdefMessage::add_uri_record(const std::string &uri) {
  auto r = std::make_shared<NdefRecordUri>(uri);
  return this->add_record(r);
}

std::vector<uint8_t> NdefMessage::encode() {
  std::vector<uint8_t> data;

  for (uint8_t i = 0; i < this->records_.size(); i++) {
    auto encoded_record = this->records_[i]->encode(i == 0, (i + 1) == this->records_.size());
    data.insert(data.end(), encoded_record.begin(), encoded_record.end());
  }
  return data;
}

}  // namespace nfc
}  // namespace esphome
