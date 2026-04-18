#include "nfc.h"

#include <algorithm>
#include <array>
#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.mifare_ultralight";

std::unique_ptr<NfcTag> Nfcc::read_mifare_ultralight_tag_(NfcTagUid &uid) {
  std::vector<uint8_t> data;
  if (!this->nfc_read_mifare_ultralight_page_(3, data)) {
    return make_unique<NfcTag>(uid, NFC_FORUM_TYPE_2);
  }

  if (!this->is_mifare_ultralight_formatted_(data)) {
    ESP_LOGW(TAG, "Not NDEF formatted");
    return make_unique<NfcTag>(uid, NFC_FORUM_TYPE_2);
  }

  uint8_t message_length;
  uint8_t message_start_index;
  if (!this->find_mifare_ultralight_ndef_(data, message_length, message_start_index)) {
    ESP_LOGW(TAG, "Couldn't find NDEF message");
    return make_unique<NfcTag>(uid, NFC_FORUM_TYPE_2);
  }

  ESP_LOGVV(TAG, "NDEF message length: %u, start: %u", message_length, message_start_index);

  if (message_length == 0) {
    return make_unique<NfcTag>(uid, NFC_FORUM_TYPE_2);
  }

  const uint16_t bytes_after_page_3 = message_length + message_start_index;
  uint16_t remaining_bytes = bytes_after_page_3 > 12 ? bytes_after_page_3 - 12 : 0;
  uint8_t current_page = MIFARE_ULTRALIGHT_DATA_START_PAGE + 3;

  while (remaining_bytes > 0) {
    std::vector<uint8_t> chunk;
    if (!this->nfc_read_mifare_ultralight_page_(current_page, chunk)) {
      ESP_LOGE(TAG, "Error reading tag data");
      return make_unique<NfcTag>(uid, NFC_FORUM_TYPE_2);
    }

    const uint8_t copy_length = std::min<uint16_t>(remaining_bytes, chunk.size());
    data.insert(data.end(), chunk.begin(), chunk.begin() + copy_length);
    remaining_bytes -= copy_length;
    current_page += MIFARE_ULTRALIGHT_READ_SIZE;
  }

  data.erase(data.begin(), data.begin() + message_start_index + MIFARE_ULTRALIGHT_PAGE_SIZE);

  return make_unique<NfcTag>(uid, NFC_FORUM_TYPE_2, data);
}

bool Nfcc::write_mifare_ultralight_tag_(NfcTagUid &uid, NdefMessage *message) {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();

  auto encoded = message->encode();

  uint32_t message_length = encoded.size();
  uint32_t buffer_length = get_mifare_ultralight_buffer_size(message_length);

  if (buffer_length > capacity) {
    ESP_LOGE(TAG, "Message length exceeds tag capacity %" PRIu32 " > %" PRIu32, buffer_length, capacity);
    return false;
  }

  encoded.insert(encoded.begin(), 0x03);
  if (message_length < 255) {
    encoded.insert(encoded.begin() + 1, message_length);
  } else {
    encoded.insert(encoded.begin() + 1, 0xFF);
    encoded.insert(encoded.begin() + 2, (message_length >> 8) & 0xFF);
    encoded.insert(encoded.begin() + 3, message_length & 0xFF);
  }
  encoded.push_back(0xFE);

  encoded.resize(buffer_length, 0);

  uint32_t index = 0;
  uint8_t current_page = MIFARE_ULTRALIGHT_DATA_START_PAGE;

  while (index < buffer_length) {
    std::vector<uint8_t> page_data(encoded.begin() + index, encoded.begin() + index + MIFARE_ULTRALIGHT_PAGE_SIZE);
    page_data.resize(MIFARE_ULTRALIGHT_PAGE_SIZE);
    if (!this->nfc_write_mifare_ultralight_page_(current_page, page_data)) {
      return false;
    }
    index += MIFARE_ULTRALIGHT_PAGE_SIZE;
    current_page++;
  }
  return true;
}

bool Nfcc::clean_mifare_ultralight_() {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();
  uint8_t pages = (capacity / MIFARE_ULTRALIGHT_PAGE_SIZE) + MIFARE_ULTRALIGHT_DATA_START_PAGE;

  static constexpr std::array<uint8_t, MIFARE_ULTRALIGHT_PAGE_SIZE> BLANK_DATA = {0x00, 0x00, 0x00, 0x00};
  const std::vector<uint8_t> blank_data(BLANK_DATA.begin(), BLANK_DATA.end());

  for (int i = MIFARE_ULTRALIGHT_DATA_START_PAGE; i < pages; i++) {
    if (!this->nfc_write_mifare_ultralight_page_(i, blank_data)) {
      return false;
    }
  }
  return true;
}

uint16_t Nfcc::read_mifare_ultralight_capacity_() {
  std::vector<uint8_t> data;
  if (this->nfc_read_mifare_ultralight_page_(3, data) && data.size() >= 3) {
    ESP_LOGV(TAG, "Tag capacity is %u bytes", data[2] * 8U);
    return data[2] * 8U;
  }
  return 0;
}

bool Nfcc::is_mifare_ultralight_formatted_(const std::vector<uint8_t> &data) {
  const uint8_t p4_offset = MIFARE_ULTRALIGHT_PAGE_SIZE;

  return (data.size() > p4_offset + 3) &&
         ((data[p4_offset + 0] != 0xFF) || (data[p4_offset + 1] != 0xFF) || (data[p4_offset + 2] != 0xFF) ||
          (data[p4_offset + 3] != 0xFF));
}

bool Nfcc::find_mifare_ultralight_ndef_(const std::vector<uint8_t> &data, uint8_t &message_length,
                                        uint8_t &message_start_index) {
  const uint8_t p4_offset = MIFARE_ULTRALIGHT_PAGE_SIZE;

  if (!(data.size() > p4_offset + 6)) {
    return false;
  }

  if (data[p4_offset] == 0x03) {
    message_length = data[p4_offset + 1];
    message_start_index = 2;
    return true;
  }
  if (data[p4_offset + 5] == 0x03) {
    message_length = data[p4_offset + 6];
    message_start_index = 7;
    return true;
  }

  return false;
}

}
}
