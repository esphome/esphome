#include <array>
#include <memory>

#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn532 {

static const char *const TAG = "pn532.mifare_ultralight";

std::unique_ptr<nfc::NfcTag> PN532::read_mifare_ultralight_tag_(nfc::NfcTagUid &uid) {
  return this->nfc::Nfcc::read_mifare_ultralight_tag_(uid);
}

bool PN532::nfc_read_mifare_ultralight_page_(uint8_t page, std::vector<uint8_t> &data) {
  data.clear();
  return this->read_mifare_ultralight_bytes_(page, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE * nfc::MIFARE_ULTRALIGHT_READ_SIZE,
                                             data);
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
  return this->nfc::Nfcc::is_mifare_ultralight_formatted_(page_3_to_6);
}

uint16_t PN532::read_mifare_ultralight_capacity_() {
  return this->nfc::Nfcc::read_mifare_ultralight_capacity_();
}

bool PN532::find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint8_t &message_length,
                                         uint8_t &message_start_index) {
  return this->nfc::Nfcc::find_mifare_ultralight_ndef_(page_3_to_6, message_length, message_start_index);
}

bool PN532::write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  return this->nfc::Nfcc::write_mifare_ultralight_tag_(uid, message);
}

bool PN532::clean_mifare_ultralight_() {
  return this->nfc::Nfcc::clean_mifare_ultralight_();
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

bool PN532::nfc_write_mifare_ultralight_page_(uint16_t page, const std::vector<uint8_t> &data) {
  if (page > 0xFF) {
    return false;
  }
  return this->write_mifare_ultralight_page_(static_cast<uint8_t>(page), data.data(), data.size());
}

}  // namespace pn532
}  // namespace esphome
