#include <array>
#include <cinttypes>
#include <memory>

#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome::pn532 {

static const char *const TAG = "pn532.mifare_ultralight";

std::unique_ptr<nfc::NfcTag> PN532::read_mifare_ultralight_tag_(nfc::NfcTagUid &uid) {
  std::vector<uint8_t> data;
  // pages 3 to 6 contain various info we are interested in -- do one read to grab it all
  if (!this->read_mifare_ultralight_bytes_(3, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE * nfc::MIFARE_ULTRALIGHT_READ_SIZE,
                                           data)) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  if (!this->is_mifare_ultralight_formatted_(data)) {
    ESP_LOGW(TAG, "Not NDEF formatted");
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  uint32_t message_length;
  uint8_t message_start_index;
  if (!this->find_mifare_ultralight_ndef_(data, message_length, message_start_index)) {
    ESP_LOGW(TAG, "Couldn't find NDEF message");
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }
  ESP_LOGVV(TAG, "NDEF message length: %" PRIu32 ", start: %u", message_length, message_start_index);

  if (message_length == 0) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  // data[2] is the CC byte read earlier as part of pages 3-6, same value used by read_mifare_ultralight_capacity_()
  const uint32_t capacity = data[2] * 8U;
  const uint32_t total_length = message_length + message_start_index;
  if (total_length > capacity) {
    ESP_LOGW(TAG, "NDEF message length %" PRIu32 " exceeds tag capacity %" PRIu32, message_length, capacity);
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }
  // we already read pages 3-6 earlier -- pick up where we left off so we're not re-reading pages
  const uint16_t read_length = total_length > 12 ? static_cast<uint16_t>(total_length - 12) : 0;
  if (read_length) {
    if (!read_mifare_ultralight_bytes_(nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE + 3, read_length, data)) {
      ESP_LOGE(TAG, "Error reading tag data");
      return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
    }
  }
  // we need to trim off page 3 as well as any bytes ahead of message_start_index
  data.erase(data.begin(), data.begin() + message_start_index + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE);

  return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2, data);
}

bool PN532::read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, std::vector<uint8_t> &data) {
  const uint8_t read_increment = nfc::MIFARE_ULTRALIGHT_READ_SIZE * nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
  std::vector<uint8_t> response;

  for (uint8_t i = 0; i * read_increment < num_bytes; i++) {
    if (!this->write_command_({
            PN532_COMMAND_INDATAEXCHANGE,
            0x01,  // One card
            nfc::MIFARE_CMD_READ,
            uint8_t(i * nfc::MIFARE_ULTRALIGHT_READ_SIZE + start_page),
        })) {
      return false;
    }

    if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response) || response[0] != 0x00) {
      return false;
    }
    uint16_t bytes_offset = (i + 1) * read_increment;
    auto pages_in_end_itr = bytes_offset <= num_bytes ? response.end() : response.end() - (bytes_offset - num_bytes);

    if ((pages_in_end_itr > response.begin()) && (pages_in_end_itr <= response.end())) {
      data.insert(data.end(), response.begin() + 1, pages_in_end_itr);
    }
  }

  char data_buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
  ESP_LOGVV(TAG, "Data read: %s", nfc::format_bytes_to(data_buf, data));

  return true;
}

bool PN532::is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_3_to_6) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  return (page_3_to_6.size() > p4_offset + 3) &&
         ((page_3_to_6[p4_offset + 0] != 0xFF) || (page_3_to_6[p4_offset + 1] != 0xFF) ||
          (page_3_to_6[p4_offset + 2] != 0xFF) || (page_3_to_6[p4_offset + 3] != 0xFF));
}

uint16_t PN532::read_mifare_ultralight_capacity_() {
  std::vector<uint8_t> data;
  if (this->read_mifare_ultralight_bytes_(3, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE, data)) {
    ESP_LOGV(TAG, "Tag capacity is %u bytes", data[2] * 8U);
    return data[2] * 8U;
  }
  return 0;
}

bool PN532::find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint32_t &message_length,
                                         uint8_t &message_start_index) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  if (!(page_3_to_6.size() > p4_offset + 6)) {
    return false;
  }

  uint8_t tlv_offset;
  if (page_3_to_6[p4_offset + 0] == 0x03) {
    tlv_offset = 0;
  } else if (page_3_to_6[p4_offset + 5] == 0x03) {
    tlv_offset = 5;
  } else {
    return false;
  }

  // Long-form TLV length: 0x03 0xFF LEN_HI LEN_LO, used whenever the message is >= 255 bytes.
  // Short-form is just 0x03 LEN. See NFC Forum Type 2 Tag Operation spec section 2.3.
  if (page_3_to_6[p4_offset + tlv_offset + 1] == 0xFF) {
    if (page_3_to_6.size() <= p4_offset + tlv_offset + 3) {
      // long-form marker present but the length bytes weren't read -- fail explicitly instead of
      // misreporting a 255-byte short-form message
      return false;
    }
    message_length =
        (static_cast<uint32_t>(page_3_to_6[p4_offset + tlv_offset + 2]) << 8) | page_3_to_6[p4_offset + tlv_offset + 3];
    message_start_index = tlv_offset + 4;
  } else {
    message_length = page_3_to_6[p4_offset + tlv_offset + 1];
    message_start_index = tlv_offset + 2;
  }
  return true;
}

bool PN532::write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();

  auto encoded = message->encode();

  uint32_t message_length = encoded.size();
  uint32_t buffer_length = nfc::get_mifare_ultralight_buffer_size(message_length);

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
  uint8_t current_page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  while (index < buffer_length) {
    if (!this->write_mifare_ultralight_page_(current_page, encoded.data() + index, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE)) {
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

  static constexpr std::array<uint8_t, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE> BLANK_DATA = {0x00, 0x00, 0x00, 0x00};

  for (int i = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE; i < pages; i++) {
    if (!this->write_mifare_ultralight_page_(i, BLANK_DATA.data(), BLANK_DATA.size())) {
      return false;
    }
  }
  return true;
}

bool PN532::write_mifare_ultralight_page_(uint8_t page_num, const uint8_t *write_data, size_t len) {
  std::vector<uint8_t> cmd({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE_ULTRALIGHT,
      page_num,
  });
  cmd.insert(cmd.end(), write_data, write_data + len);
  if (!this->write_command_(cmd)) {
    ESP_LOGE(TAG, "Error writing page %u", page_num);
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response)) {
    ESP_LOGE(TAG, "Error writing page %u", page_num);
    return false;
  }

  return true;
}

}  // namespace esphome::pn532
