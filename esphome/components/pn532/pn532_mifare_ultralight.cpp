#include <algorithm>
#include <array>
#include <cinttypes>
#include <memory>

#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome::pn532 {

ESPHOME_LOG_TAG(TAG, "pn532.mifare_ultralight");

std::unique_ptr<nfc::NfcTag> PN532::read_mifare_ultralight_tag_(nfc::NfcTagUid &uid) {
  UltralightHeader data;
  // pages 3 to 6 contain various info we are interested in -- do one read to grab it all
  if (!this->read_mifare_ultralight_bytes_(3, data)) {
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
    ESP_LOGW(TAG, "NDEF message of %" PRIu32 " bytes at offset %u exceeds tag capacity %" PRIu32, message_length,
             message_start_index, capacity);
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }
  if (message_length > MIFARE_ULTRALIGHT_MAX_NDEF_SIZE) {
    ESP_LOGE(TAG, "NDEF message too long: %" PRIu32 " bytes", message_length);
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  // we already read pages 3-6 earlier, pick up where we left off so we're not re-reading pages
  constexpr size_t header_message_bytes = MIFARE_READ_SIZE - nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // pages 4 to 6
  const uint16_t read_length =
      total_length > header_message_bytes ? static_cast<uint16_t>(total_length - header_message_bytes) : 0;
  // A message up to the old 255 byte limit stays on the stack, a longer one borrows the heap for the read
  SmallBufferWithHeapFallback<MIFARE_READ_SIZE + 256> buffer(data.size() + read_length);
  std::span<uint8_t> message(buffer.get(), data.size() + read_length);
  std::copy(data.begin(), data.end(), message.begin());
  if (read_length) {
    if (!this->read_mifare_ultralight_bytes_(nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE + 3,
                                             message.subspan(data.size()))) {
      ESP_LOGE(TAG, "Error reading tag data");
      return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
    }
  }
  // skip page 3 as well as any bytes ahead of message_start_index
  const size_t skip = message_start_index + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
  if (skip >= message.size()) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2,
                                  make_unique<nfc::NdefMessage>(std::span<const uint8_t>(message).subspan(skip)));
}

bool PN532::read_mifare_ultralight_bytes_(uint8_t start_page, std::span<uint8_t> data) {
  MifareReadData chunk;

  for (uint8_t i = 0; i * MIFARE_READ_SIZE < data.size(); i++) {
    // a READ returns 4 pages (16 bytes)
    if (!this->mifare_read_(uint8_t(i * nfc::MIFARE_ULTRALIGHT_READ_SIZE + start_page), chunk)) {
      return false;
    }
    // keep only the bytes still wanted from this read
    const size_t offset = i * MIFARE_READ_SIZE;
    const size_t count = std::min(MIFARE_READ_SIZE, data.size() - offset);
    std::copy_n(chunk.begin(), count, data.begin() + offset);
  }

  char data_buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
  ESP_LOGVV(TAG, "Data read: %s", nfc::format_bytes_to(data_buf, data));

  return true;
}

bool PN532::is_mifare_ultralight_formatted_(const std::span<const uint8_t> page_3_to_6) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  return (page_3_to_6.size() > p4_offset + 3) &&
         ((page_3_to_6[p4_offset + 0] != 0xFF) || (page_3_to_6[p4_offset + 1] != 0xFF) ||
          (page_3_to_6[p4_offset + 2] != 0xFF) || (page_3_to_6[p4_offset + 3] != 0xFF));
}

uint16_t PN532::read_mifare_ultralight_capacity_() {
  std::array<uint8_t, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE> data;
  if (this->read_mifare_ultralight_bytes_(3, data)) {
    ESP_LOGV(TAG, "Tag capacity is %u bytes", data[2] * 8U);
    return data[2] * 8U;
  }
  return 0;
}

bool PN532::find_mifare_ultralight_ndef_(const std::span<const uint8_t> page_3_to_6, uint32_t &message_length,
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

  const auto encoded = message->encode();
  const uint32_t buffer_length = nfc::get_mifare_ultralight_buffer_size(encoded.size());

  if (buffer_length > capacity) {
    ESP_LOGE(TAG, "Message length exceeds tag capacity %" PRIu32 " > %" PRIu32, buffer_length, capacity);
    return false;
  }

  FixedVector<uint8_t> buffer;
  nfc::fill_ndef_tlv(encoded, buffer_length, buffer);

  uint32_t index = 0;
  uint8_t current_page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  while (index < buffer_length) {
    if (!this->write_mifare_ultralight_page_(
            current_page, std::span<const uint8_t>(&buffer[index], nfc::MIFARE_ULTRALIGHT_PAGE_SIZE))) {
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
    if (!this->write_mifare_ultralight_page_(i, BLANK_DATA)) {
      return false;
    }
  }
  return true;
}

bool PN532::write_mifare_ultralight_page_(uint8_t page_num, const std::span<const uint8_t> write_data) {
  StaticVector<uint8_t, 4 + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE> cmd = {
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE_ULTRALIGHT,
      page_num,
  };
  for (const uint8_t byte : write_data) {
    cmd.push_back(byte);
  }

  PN532Frame response;
  if (!this->in_data_exchange_(cmd, response)) {
    ESP_LOGE(TAG, "Error writing page %u", page_num);
    return false;
  }

  return true;
}

}  // namespace esphome::pn532
