#include <cinttypes>
#include <memory>

#include "pn7160.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn7160 {

static const char *const TAG = "pn7160.mifare_ultralight";

uint8_t PN7160::read_mifare_ultralight_tag_(nfc::NfcTag &tag) {
  std::vector<uint8_t> data;
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
  // we need to trim off page 3 as well as any bytes ahead of message_start_index
  data.erase(data.begin(), data.begin() + message_start_index + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE);

  tag.set_ndef_message(make_unique<nfc::NdefMessage>(data));

  return nfc::STATUS_OK;
}

uint8_t PN7160::read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, std::vector<uint8_t> &data) {
  const uint8_t read_increment = nfc::MIFARE_ULTRALIGHT_READ_SIZE * nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_DATA, {nfc::MIFARE_CMD_READ, start_page});

  for (size_t i = 0; i * read_increment < num_bytes; i++) {
    tx.get_message().back() = i * nfc::MIFARE_ULTRALIGHT_READ_SIZE + start_page;
    do {  // loop because sometimes we struggle here...???...
      if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Error reading tag data");
        return nfc::STATUS_FAILED;
      }
    } while (rx.get_payload_size() < read_increment);
    uint16_t bytes_offset = (i + 1) * read_increment;
    auto pages_in_end_itr = bytes_offset <= num_bytes ? rx.get_message().end() - 1
                                                      : rx.get_message().end() - (bytes_offset - num_bytes + 1);

    if ((pages_in_end_itr > rx.get_message().begin()) && (pages_in_end_itr < rx.get_message().end())) {
      data.insert(data.end(), rx.get_message().begin() + nfc::NCI_PKT_HEADER_SIZE, pages_in_end_itr);
    }
  }

  ESP_LOGVV(TAG, "Data read: %s", nfc::format_bytes(data).c_str());

  return nfc::STATUS_OK;
}

bool PN7160::is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_3_to_6) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  return (page_3_to_6.size() > p4_offset + 3) &&
         !((page_3_to_6[p4_offset + 0] == 0xFF) && (page_3_to_6[p4_offset + 1] == 0xFF) &&
           (page_3_to_6[p4_offset + 2] == 0xFF) && (page_3_to_6[p4_offset + 3] == 0xFF));
}

uint16_t PN7160::read_mifare_ultralight_capacity_() {
  std::vector<uint8_t> data;
  if (this->read_mifare_ultralight_bytes_(3, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE, data) == nfc::STATUS_OK) {
    ESP_LOGV(TAG, "Tag capacity is %u bytes", data[2] * 8U);
    return data[2] * 8U;
  }
  return 0;
}

uint8_t PN7160::find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint8_t &message_length,
                                             uint8_t &message_start_index) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;  // page 4 will begin 4 bytes into the vector

  if (!(page_3_to_6.size() > p4_offset + 5)) {
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

uint8_t PN7160::write_mifare_ultralight_tag_(std::vector<uint8_t> &uid,
                                             const std::shared_ptr<nfc::NdefMessage> &message) {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();

  auto encoded = message->encode();

  uint32_t message_length = encoded.size();
  uint32_t buffer_length = nfc::get_mifare_ultralight_buffer_size(message_length);

  if (buffer_length > capacity) {
    ESP_LOGE(TAG, "Message length exceeds tag capacity %" PRIu32 " > %" PRIu32, buffer_length, capacity);
    return nfc::STATUS_FAILED;
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
    if (this->write_mifare_ultralight_page_(current_page, data) != nfc::STATUS_OK) {
      return nfc::STATUS_FAILED;
    }
    index += nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
    current_page++;
  }
  return nfc::STATUS_OK;
}

uint8_t PN7160::clean_mifare_ultralight_() {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();
  uint8_t pages = (capacity / nfc::MIFARE_ULTRALIGHT_PAGE_SIZE) + nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  std::vector<uint8_t> blank_data = {0x00, 0x00, 0x00, 0x00};

  for (int i = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE; i < pages; i++) {
    if (this->write_mifare_ultralight_page_(i, blank_data) != nfc::STATUS_OK) {
      return nfc::STATUS_FAILED;
    }
  }
  return nfc::STATUS_OK;
}

uint8_t PN7160::write_mifare_ultralight_page_(uint8_t page_num, std::vector<uint8_t> &write_data) {
  std::vector<uint8_t> payload = {nfc::MIFARE_CMD_WRITE_ULTRALIGHT, page_num};
  payload.insert(payload.end(), write_data.begin(), write_data.end());

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_DATA, payload);

  if (this->transceive_(tx, rx, NFCC_TAG_WRITE_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error writing page %u", page_num);
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

}  // namespace pn7160
}  // namespace esphome
