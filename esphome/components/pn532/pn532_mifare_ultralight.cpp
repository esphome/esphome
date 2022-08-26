#include <memory>

#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn532 {

static const char *const TAG = "pn532.mifare_ultralight";

std::unique_ptr<nfc::NfcTag> PN532::read_mifare_ultralight_tag(std::vector<uint8_t> &uid) {
  if (!this->is_mifare_ultralight_formatted_()) {
    ESP_LOGD(TAG, "Not NDEF formatted");
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  uint8_t message_length;
  uint8_t message_start_index;
  if (!this->find_mifare_ultralight_ndef_(message_length, message_start_index)) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }
  ESP_LOGVV(TAG, "message length: %d, start: %d", message_length, message_start_index);

  if (message_length == 0) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }
  std::vector<uint8_t> data;
  for (uint8_t page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE; page < nfc::MIFARE_ULTRALIGHT_MAX_PAGE; page++) {
    std::vector<uint8_t> page_data;
    if (!this->read_mifare_ultralight_page_(page, page_data)) {
      ESP_LOGE(TAG, "Error reading page %d", page);
      return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
    }
    data.insert(data.end(), page_data.begin(), page_data.end());

    if (data.size() >= (message_length + message_start_index))
      break;
  }

  data.erase(data.begin(), data.begin() + message_start_index);
  data.erase(data.begin() + message_length, data.end());

  return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2, data);
}

bool PN532::read_mifare_ultralight_page(uint8_t page_num, std::vector<uint8_t> &data) {
  if (!this->write_command_({
          PN532_COMMAND_INDATAEXCHANGE,
          0x01,  // One card
          nfc::MIFARE_CMD_READ,
          page_num,
      })) {
    return false;
  }

  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, data) || data[0] != 0x00) {
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

uint16_t PN532::read_mifare_ultralight_capacity_() {
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

bool PN532::write_mifare_ultralight_tag(std::vector<uint8_t> &uid, nfc::NdefMessage *message) {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();

  auto encoded = message->encode();

  uint32_t message_length = encoded.size();
  uint32_t buffer_length = nfc::get_mifare_ultralight_buffer_size(message_length);

  if (buffer_length > capacity) {
    ESP_LOGE(TAG, "Message length exceeds tag capacity %d > %d", buffer_length, capacity);
    return false;
  }

  encoded.insert(encoded.begin(), 0x03);
  if (message_length < 255) {
    encoded.insert(encoded.begin() + 1, message_length);
  } else {
    encoded.insert(encoded.begin() + 1, 0xFF);
    encoded.insert(encoded.begin() + 2, (message_length >> 8) & 0xFF);
    encoded.insert(encoded.begin() + 2, message_length & 0xFF);
  }
  encoded.push_back(0xFE);

  encoded.resize(buffer_length, 0);

  uint32_t index = 0;
  uint8_t current_page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  while (index < buffer_length) {
    std::vector<uint8_t> data(encoded.begin() + index, encoded.begin() + index + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE);
    if (!this->write_mifare_ultralight_page_(current_page, data)) {
      return false;
    }
    index += nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
    current_page++;
  }
  return true;
}

bool PN532::clean_mifare_ultralight_() {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();
  uint8_t pages = (capacity / nfc::MIFARE_ULTRALIGHT_PAGE_SIZE) + nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  std::vector<uint8_t> blank_data = {0x00, 0x00, 0x00, 0x00};

  for (int i = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE; i < pages; i++) {
    if (!this->write_mifare_ultralight_page_(i, blank_data)) {
      return false;
    }
  }
  return true;
}

bool PN532::write_mifare_ultralight_page(uint8_t page_num, std::vector<uint8_t> &write_data) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE_ULTRALIGHT,
      page_num,
  });
  data.insert(data.end(), write_data.begin(), write_data.end());
  if (!this->write_command_(data)) {
    ESP_LOGE(TAG, "Error writing page %d", page_num);
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response)) {
    ESP_LOGE(TAG, "Error writing page %d", page_num);
    return false;
  }

  return true;
}

}  // namespace pn532
}  // namespace esphome
