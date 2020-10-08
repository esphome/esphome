#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn532 {

static const char *TAG = "pn532.mifare";

std::vector<uint8_t> PN532::read_mifare_classic_block_(uint8_t block_num) {
  if (!this->write_command_({
          PN532_COMMAND_INDATAEXCHANGE,
          0x01,  // One card
          nfc::MIFARE_CMD_READ,
          block_num,
      })) {
    return {};
  }

  std::vector<uint8_t> response;
  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, response) || response[0] != 0x00) {
    return {};
  }
  response.erase(response.begin());
  return response;
}

bool PN532::auth_mifare_classic_block_(std::vector<uint8_t> &uid, uint8_t block_num, uint8_t key_num,
                                       const uint8_t *key) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,       // One card
      key_num,    // Mifare Key slot
      block_num,  // Block number
  });
  data.insert(data.end(), key, key + 6);
  data.insert(data.end(), uid.begin(), uid.end());
  if (!this->write_command_(data)) {
    ESP_LOGE(TAG, "Authentication failed");
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, response) || response[0] != 0x00) {
    ESP_LOGE(TAG, "Authentication failed");
    return false;
  }

  return true;
}


bool PN532::format_mifare_classic_mifare_(std::vector<uint8_t> &uid) {
  std::vector<uint8_t> blank_buffer(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  std::vector<uint8_t> trailer_buffer(
      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x80, 0x69, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  bool error = false;

  for (int idx = 0; idx < 16; idx++) {
    if (!this->auth_mifare_classic_block_(uid, (4 * idx) + 3, nfc::MIFARE_CMD_AUTH_B, nfc::DEFAULT_KEY)) {
      ESP_LOGE(TAG, "No keys work!!! sector %d", idx);
      continue;
    }

    if (idx == 0) {
      if (!this->write_mifare_classic_block_((4 * idx) + 1, blank_buffer)) {
        ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx) + 1);
        error = true;
      }
    } else {
      if (!this->write_mifare_classic_block_((4 * idx), blank_buffer)) {
        ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx));
        error = true;
      }
      if (!this->write_mifare_classic_block_((4 * idx) + 1, blank_buffer)) {
        ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx) + 1);
        error = true;
      }
    }

    if (!this->write_mifare_classic_block_((4 * idx) + 2, blank_buffer)) {
      ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx) + 2);
      error = true;
    }

    if (!this->write_mifare_classic_block_((4 * idx) + 3, trailer_buffer)) {
      ESP_LOGE(TAG, "Unable to write trailer of sector %d-%d", idx, (4 * idx) + 3);
      error = true;
    }
  }

  return !error;
}

bool PN532::format_mifare_classic_ndef_(std::vector<uint8_t> &uid) {
  std::vector<uint8_t> empty_ndef_message(
      {0x03, 0x03, 0xD0, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  std::vector<uint8_t> sector_buffer_0(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  std::vector<uint8_t> sector_buffer_1(
      {0x14, 0x01, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1});
  std::vector<uint8_t> sector_buffer_2(
      {0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1});
  std::vector<uint8_t> sector_buffer_3(
      {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x78, 0x77, 0x88, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  std::vector<uint8_t> sector_buffer_4(
      {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07, 0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  if (!this->auth_mifare_classic_block_(uid, 0, nfc::MIFARE_CMD_AUTH_A, nfc::DEFAULT_KEY)) {
    ESP_LOGE(TAG, "Unable to authenticate block 0 for formatting!");
    return false;
  }
  if (!this->write_mifare_classic_block_(1, sector_buffer_1))
    return false;
  if (!this->write_mifare_classic_block_(2, sector_buffer_2))
    return false;
  if (!this->write_mifare_classic_block_(3, sector_buffer_3))
    return false;

  for (int i = 4; i < 64; i += 4) {
    if (!this->auth_mifare_classic_block_(uid, i, nfc::MIFARE_CMD_AUTH_A, nfc::DEFAULT_KEY)) {
      ESP_LOGE(TAG, "Failed to authenticate with block %d", i);
      continue;
    }
    if (i == 4) {
      if (!this->write_mifare_classic_block_(i, empty_ndef_message))
        ESP_LOGE(TAG, "Unable to write block %d", i);
    } else {
      if (!this->write_mifare_classic_block_(i, sector_buffer_0))
        ESP_LOGE(TAG, "Unable to write block %d", i);
    }
    if (!this->write_mifare_classic_block_(i + 1, sector_buffer_0))
      ESP_LOGE(TAG, "Unable to write block %d", i + 1);
    if (!this->write_mifare_classic_block_(i + 2, sector_buffer_0))
      ESP_LOGE(TAG, "Unable to write block %d", i + 2);
    if (!this->write_mifare_classic_block_(i + 3, sector_buffer_4))
      ESP_LOGE(TAG, "Unable to write block %d", i + 3);
  }
  return true;
}

bool PN532::write_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &write_data) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE,
      block_num,  // Block number
  });
  data.insert(data.end(), write_data.begin(), write_data.end());
  if (!this->write_command_(data)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, response)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  return true;
}

}  // namespace pn532
}  // namespace esphome
