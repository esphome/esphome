#include <memory>

#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn532 {

static const char *const TAG = "pn532.mifare_classic";

std::unique_ptr<nfc::NfcTag> PN532::read_mifare_classic_tag_(nfc::NfcTagUid &uid) {
  return this->Nfcc::read_mifare_classic_tag_(uid);
}

bool PN532::read_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data) {
  if (!this->write_command_({
          PN532_COMMAND_INDATAEXCHANGE,
          0x01,  // One card
          nfc::MIFARE_CMD_READ,
          block_num,
      })) {
    return false;
  }

  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, data) || data[0] != 0x00) {
    return false;
  }
  data.erase(data.begin());

  char data_buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
  ESP_LOGVV(TAG, " Block %d: %s", block_num, nfc::format_bytes_to(data_buf, data));
  return true;
}

bool PN532::auth_mifare_classic_block_(nfc::NfcTagUid &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,       // One card
      key_num,    // Mifare Key slot
      block_num,  // Block number
  });
  data.insert(data.end(), key, key + 6);
  data.insert(data.end(), uid.begin(), uid.end());
  if (!this->write_command_(data)) {
    ESP_LOGE(TAG, "Authentication failed - Block %d", block_num);
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response) || response[0] != 0x00) {
    ESP_LOGE(TAG, "Authentication failed - Block 0x%02x", block_num);
    return false;
  }

  return true;
}

bool PN532::nfc_auth_mifare_classic_block_(const nfc::NfcTagUid &uid, uint8_t block, uint8_t key_type,
                                           const uint8_t *key) {
  auto uid_copy = uid;
  return this->auth_mifare_classic_block_(uid_copy, block, key_type, key);
}

bool PN532::format_mifare_classic_mifare_(nfc::NfcTagUid &uid) {
  return this->Nfcc::format_mifare_classic_mifare_(uid);
}

bool PN532::format_mifare_classic_ndef_(nfc::NfcTagUid &uid) { return this->Nfcc::format_mifare_classic_ndef_(uid); }

bool PN532::write_mifare_classic_block_(uint8_t block_num, const uint8_t *data, size_t len) {
  std::vector<uint8_t> cmd({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE,
      block_num,
  });
  cmd.insert(cmd.end(), data, data + len);
  if (!this->write_command_(cmd)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  return true;
}

bool PN532::nfc_read_mifare_classic_block_(uint8_t block, std::vector<uint8_t> &data) {
  return this->read_mifare_classic_block_(block, data);
}

bool PN532::nfc_write_mifare_classic_block_(uint8_t block, const std::vector<uint8_t> &data) {
  return this->write_mifare_classic_block_(block, data.data(), data.size());
}

bool PN532::write_mifare_classic_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  return this->Nfcc::write_mifare_classic_tag_(uid, message);
}

}  // namespace pn532
}  // namespace esphome
