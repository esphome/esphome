#include <algorithm>
#include <array>
#include <cinttypes>
#include <memory>

#include "pn71xx.h"
#include "esphome/core/log.h"

namespace esphome::pn71xx {

static const char *const TAG = "pn71xx.mifare_ultralight";

uint8_t PN71xx::read_mifare_ultralight_tag_(nfc::NfcTag &tag) {
  UltralightReadBuffer data;
  // pages 3 to 6 contain various info we are interested in -- do one read to grab it all
  if (this->read_mifare_ultralight_bytes_(3, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE * nfc::MIFARE_ULTRALIGHT_READ_SIZE,
                                          data) != nfc::STATUS_OK) {
    return nfc::STATUS_FAILED;
  }

  if (!this->is_mifare_ultralight_formatted_(data)) {
    ESP_LOGW(TAG, "Not NDEF formatted");
    return nfc::STATUS_FAILED;
  }

  uint8_t message_length;
  uint8_t message_start_index;
  if (this->find_mifare_ultralight_ndef_(data, message_length, message_start_index) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "Couldn't find NDEF message");
    return nfc::STATUS_FAILED;
  }
  ESP_LOGVV(TAG, "NDEF message length: %u, start: %u", message_length, message_start_index);

  if (message_length == 0) {
    return nfc::STATUS_FAILED;
  }
  // we already read pages 3-6 earlier -- pick up where we left off so we're not re-reading pages
  const uint8_t read_length = message_length + message_start_index > 12 ? message_length + message_start_index - 12 : 0;
  if (read_length) {
    if (read_mifare_ultralight_bytes_(nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE + 3, read_length, data) !=
        nfc::STATUS_OK) {
      ESP_LOGE(TAG, "Error reading tag data");
      return nfc::STATUS_FAILED;
    }
  }
  // skip page 3 as well as any bytes ahead of message_start_index
  const size_t skip = message_start_index + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
  if (skip >= data.size()) {
    return nfc::STATUS_FAILED;
  }
  tag.set_ndef_message(make_unique<nfc::NdefMessage>(std::span<const uint8_t>(data).subspan(skip)));

  return nfc::STATUS_OK;
}

uint8_t PN71xx::read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, UltralightReadBuffer &data) {
  const uint8_t read_increment = nfc::MIFARE_ULTRALIGHT_READ_SIZE * nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_DATA, {nfc::MIFARE_CMD_READ, start_page});

  for (size_t i = 0; i * read_increment < num_bytes; i++) {
    const uint8_t page = i * nfc::MIFARE_ULTRALIGHT_READ_SIZE + start_page;
    tx.set_payload({nfc::MIFARE_CMD_READ, page});
    // a short answer (e.g. a NAK for a page beyond the end of the tag) is retried a limited number of times
    uint8_t attempts = 0;
    do {
      if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Error reading tag data");
        return nfc::STATUS_FAILED;
      }
      if (++attempts > NFCC_MAX_COMM_FAILS && rx.get_payload_size() < read_increment) {
        ESP_LOGE(TAG, "Short read from page %u", page);
        return nfc::STATUS_FAILED;
      }
    } while (rx.get_payload_size() < read_increment);
    // the payload ends with a status byte; keep only the bytes still wanted from this read
    const uint16_t wanted = num_bytes - i * read_increment;
    const size_t count = std::min<size_t>(read_increment, wanted);
    for (const uint8_t byte : rx.get_payload().subspan(0, count)) {
      data.push_back(byte);
    }
  }

  char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
  ESP_LOGVV(TAG, "Data read: %s", nfc::format_bytes_to(buf, data));

  return nfc::STATUS_OK;
}

bool PN71xx::is_mifare_ultralight_formatted_(const std::span<const uint8_t> page_3_to_6) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  return (page_3_to_6.size() > p4_offset + 3) &&
         ((page_3_to_6[p4_offset + 0] != 0xFF) || (page_3_to_6[p4_offset + 1] != 0xFF) ||
          (page_3_to_6[p4_offset + 2] != 0xFF) || (page_3_to_6[p4_offset + 3] != 0xFF));
}

uint16_t PN71xx::read_mifare_ultralight_capacity_() {
  UltralightReadBuffer data;
  if (this->read_mifare_ultralight_bytes_(3, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE, data) == nfc::STATUS_OK) {
    ESP_LOGV(TAG, "Tag capacity is %u bytes", data[2] * 8U);
    return data[2] * 8U;
  }
  return 0;
}

uint8_t PN71xx::find_mifare_ultralight_ndef_(const std::span<const uint8_t> page_3_to_6, uint8_t &message_length,
                                             uint8_t &message_start_index) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  if (!(page_3_to_6.size() > p4_offset + 6)) {
    return nfc::STATUS_FAILED;
  }

  if (page_3_to_6[p4_offset + 0] == 0x03) {
    message_length = page_3_to_6[p4_offset + 1];
    message_start_index = 2;
    return nfc::STATUS_OK;
  } else if (page_3_to_6[p4_offset + 5] == 0x03) {
    message_length = page_3_to_6[p4_offset + 6];
    message_start_index = 7;
    return nfc::STATUS_OK;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN71xx::write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, const std::shared_ptr<nfc::NdefMessage> &message) {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();

  const auto encoded = message->encode();
  const uint32_t buffer_length = nfc::get_mifare_ultralight_buffer_size(encoded.size());

  if (buffer_length > capacity) {
    ESP_LOGE(TAG, "Message length exceeds tag capacity %" PRIu32 " > %" PRIu32, buffer_length, capacity);
    return nfc::STATUS_FAILED;
  }

  FixedVector<uint8_t> buffer;
  fill_ndef_tlv_(encoded, buffer_length, buffer);

  uint32_t index = 0;
  uint8_t current_page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  while (index < buffer_length) {
    if (this->write_mifare_ultralight_page_(current_page, &buffer[index], nfc::MIFARE_ULTRALIGHT_PAGE_SIZE) !=
        nfc::STATUS_OK) {
      return nfc::STATUS_FAILED;
    }
    index += nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
    current_page++;
  }
  return nfc::STATUS_OK;
}

uint8_t PN71xx::clean_mifare_ultralight_() {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();
  uint8_t pages = (capacity / nfc::MIFARE_ULTRALIGHT_PAGE_SIZE) + nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  static constexpr std::array<uint8_t, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE> BLANK_DATA = {0x00, 0x00, 0x00, 0x00};

  for (int i = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE; i < pages; i++) {
    if (this->write_mifare_ultralight_page_(i, BLANK_DATA.data(), BLANK_DATA.size()) != nfc::STATUS_OK) {
      return nfc::STATUS_FAILED;
    }
  }
  return nfc::STATUS_OK;
}

uint8_t PN71xx::write_mifare_ultralight_page_(uint8_t page_num, const uint8_t *write_data, size_t len) {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_DATA, {nfc::MIFARE_CMD_WRITE_ULTRALIGHT, page_num});
  tx.append(std::span<const uint8_t>(write_data, len));

  if (this->transceive_(tx, rx, NFCC_TAG_WRITE_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error writing page %u", page_num);
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

}  // namespace esphome::pn71xx
