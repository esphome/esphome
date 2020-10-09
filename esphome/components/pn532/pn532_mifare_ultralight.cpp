#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn532 {

static const char *TAG = "pn532.mifare_ultralight";

nfc::NfcTag *PN532::read_mifare_ultralight_tag_(std::vector<uint8_t> &uid) {
  if (!this->is_mifare_ultralight_formatted_()) {
    ESP_LOGD(TAG, "Not NDEF formatted");
    return new nfc::NfcTag(uid, nfc::NFC_FORUM_TYPE_2);
  }

  uint8_t message_length;
  uint8_t message_start_index;
  if (!this->find_mifare_ultralight_ndef_(message_length, message_start_index)) {
    return new nfc::NfcTag(uid, nfc::NFC_FORUM_TYPE_2);
  }

  if (message_length == 0) {
    auto message = new nfc::NdefMessage();
    message->add_empty_record();
    return new nfc::NfcTag(uid, nfc::NFC_FORUM_TYPE_2, message);
  }
  std::vector<uint8_t> data;
  uint8_t index = 0;
  for (uint8_t page = 4; page < 63; page++) {
    std::vector<uint8_t> page_data;
    if (!this->read_mifare_ultralight_page_(page, page_data)) {
      ESP_LOGE(TAG, "Error reading page %d", page);
      message_length = 0;
      break;
    }
    data.insert(data.end(), page_data.begin(), page_data.end());

    if (index >= (message_length + message_start_index))
      break;

    index += page_data.size();
  }

  data.erase(data.begin(), data.begin() + message_start_index);

  return new nfc::NfcTag(uid, nfc::NFC_FORUM_TYPE_2, data);
}

bool PN532::read_mifare_ultralight_page_(uint8_t page_num, std::vector<uint8_t> &data) {
  if (!this->write_command_({
          PN532_COMMAND_INDATAEXCHANGE,
          0x01,  // One card
          nfc::MIFARE_CMD_READ,
          page_num,
      })) {
    return false;
  }

  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, data) || data[0] != 0x00) {
    return false;
  }
  data.erase(data.begin());
  // We only want 1 page of data but the PN532 returns 4 at once.
  data.erase(data.begin() + 4, data.end());

  ESP_LOGVV(TAG, "Pages %d-%d: %s", page_num, page_num + 4, nfc::format_bytes(data).c_str());

  return true;
}

bool PN532::is_mifare_ultralight_formatted_() {
  std::vector<uint8_t> data;
  if (this->read_mifare_ultralight_page_(4, data)) {
    return !(data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF);
  }
  return true;
}

uint16_t PN532::read_mifare_ultralight_capacity() {
  std::vector<uint8_t> data;
  if (this->read_mifare_ultralight_page_(3, data)) {
    return data[2] * 8U;
  }
  return 0;
}

bool PN532::find_mifare_ultralight_ndef_(uint8_t &message_length, uint8_t &message_start_index) {
  std::vector<uint8_t> data;
  for (int page = 4; page < 6; page++) {
    std::vector<uint8_t> page_data;
    if (!this->read_mifare_ultralight_page_(page, page_data)) {
      return false;
    }
    data.insert(data.end(), page_data.begin(), page_data.end());
  }
  if (data[0] == 0x03) {
    message_length = data[1];
    message_start_index = 2;
    return true;
  } else if (data[5] == 0x03) {
    message_length = data[6];
    message_start_index = 7;
    return true;
  }
  return false;
}




// bool PN532::write_mifare_ultralight_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message) {
//   auto encoded = message->encode();

//   if (encoded.size() < 255) {
//     encoded.insert(encoded.begin(), 0x03);
//     encoded.insert(encoded.begin() + 1, encoded.size());
//     encoded.push_back(0xFE);
//   } else {
//     encoded.insert(encoded.begin(), 0x03);
//     encoded.insert(encoded.begin() + 1, 0xFF);
//     encoded.insert(encoded.begin() + 2, (encoded.size() >> 8) & 0xFF);
//     encoded.insert(encoded.begin() + 2, encoded.size() & 0xFF);
//     encoded.push_back(0xFE);
//   }

//   uint32_t index = 0;
//   uint8_t current_page = 4;

//   while (index < encoded.size()) {
//     std::vector<uint8_t> data(encoded.begin() + index, encoded.begin() + index + nfc::BLOCK_SIZE);
//     this->write_mifare_ultralight_page_(current_page,)
//   }
//   return true;
// }

}  // namespace pn532
}  // namespace esphome
