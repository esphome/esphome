#include "nfc.h"

#include <array>

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc";

static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> BLANK_BUFFER = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> TRAILER_BUFFER = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x80, 0x69, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> EMPTY_NDEF_MESSAGE = {
    0x03, 0x03, 0xD0, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> BLOCK_1_DATA = {
    0x14, 0x01, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> BLOCK_2_DATA = {
    0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> BLOCK_3_TRAILER = {
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x78, 0x77, 0x88, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static constexpr std::array<uint8_t, MIFARE_CLASSIC_BLOCK_SIZE> NDEF_TRAILER = {
    0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07, 0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

std::unique_ptr<NfcTag> Nfcc::read_mifare_classic_tag_(NfcTagUid &uid) {
  uint8_t current_block = 4;
  uint8_t message_start_index = 0;
  uint32_t message_length = 0;

  if (this->nfc_auth_mifare_classic_block_(uid, current_block, MIFARE_CMD_AUTH_A, NDEF_KEY)) {
    std::vector<uint8_t> data;
    if (this->nfc_read_mifare_classic_block_(current_block, data)) {
      if (!decode_mifare_classic_tlv(data, message_length, message_start_index)) {
        return make_unique<NfcTag>(uid, ERROR);
      }
    } else {
      ESP_LOGE(TAG, "Failed to read block %d", current_block);
      return make_unique<NfcTag>(uid, MIFARE_CLASSIC);
    }
  } else {
    ESP_LOGV(TAG, "Tag is not NDEF formatted");
    return make_unique<NfcTag>(uid, MIFARE_CLASSIC);
  }

  uint32_t index = 0;
  uint32_t buffer_size = get_mifare_classic_buffer_size(message_length);
  std::vector<uint8_t> buffer;

  while (index < buffer_size) {
    if (mifare_classic_is_first_block(current_block)) {
      if (!this->nfc_auth_mifare_classic_block_(uid, current_block, MIFARE_CMD_AUTH_A, NDEF_KEY)) {
        ESP_LOGE(TAG, "Error, Block authentication failed for %d", current_block);
        return make_unique<NfcTag>(uid, MIFARE_CLASSIC);
      }
    }
    std::vector<uint8_t> block_data;
    if (this->nfc_read_mifare_classic_block_(current_block, block_data)) {
      buffer.insert(buffer.end(), block_data.begin(), block_data.end());
    } else {
      ESP_LOGE(TAG, "Error reading block %d", current_block);
      return make_unique<NfcTag>(uid, MIFARE_CLASSIC);
    }

    index += MIFARE_CLASSIC_BLOCK_SIZE;
    current_block++;

    if (mifare_classic_is_trailer_block(current_block)) {
      current_block++;
    }
  }

  if (buffer.begin() + message_start_index < buffer.end()) {
    buffer.erase(buffer.begin(), buffer.begin() + message_start_index);
  } else {
    return make_unique<NfcTag>(uid, MIFARE_CLASSIC);
  }

  return make_unique<NfcTag>(uid, MIFARE_CLASSIC, buffer);
}

bool Nfcc::format_mifare_classic_mifare_(NfcTagUid &uid) {
  bool error = false;
  std::vector<uint8_t> block_data;
  block_data.reserve(MIFARE_CLASSIC_BLOCK_SIZE);

  for (int block = 0; block < 64; block += 4) {
    if (!this->nfc_auth_mifare_classic_block_(uid, block + 3, MIFARE_CMD_AUTH_B, DEFAULT_KEY)) {
      continue;
    }
    if (block != 0) {
      block_data.assign(BLANK_BUFFER.begin(), BLANK_BUFFER.end());
      if (!this->nfc_write_mifare_classic_block_(block, block_data)) {
        ESP_LOGE(TAG, "Unable to write block %d", block);
        error = true;
      }
    }
    block_data.assign(BLANK_BUFFER.begin(), BLANK_BUFFER.end());
    if (!this->nfc_write_mifare_classic_block_(block + 1, block_data)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 1);
      error = true;
    }
    block_data.assign(BLANK_BUFFER.begin(), BLANK_BUFFER.end());
    if (!this->nfc_write_mifare_classic_block_(block + 2, block_data)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 2);
      error = true;
    }
    block_data.assign(TRAILER_BUFFER.begin(), TRAILER_BUFFER.end());
    if (!this->nfc_write_mifare_classic_block_(block + 3, block_data)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 3);
      error = true;
    }
  }

  return !error;
}

bool Nfcc::format_mifare_classic_ndef_(NfcTagUid &uid) {
  std::vector<uint8_t> block_data;
  block_data.reserve(MIFARE_CLASSIC_BLOCK_SIZE);

  if (!this->nfc_auth_mifare_classic_block_(uid, 0, MIFARE_CMD_AUTH_B, DEFAULT_KEY)) {
    ESP_LOGE(TAG, "Unable to authenticate block 0 for formatting!");
    return false;
  }
  block_data.assign(BLOCK_1_DATA.begin(), BLOCK_1_DATA.end());
  if (!this->nfc_write_mifare_classic_block_(1, block_data))
    return false;
  block_data.assign(BLOCK_2_DATA.begin(), BLOCK_2_DATA.end());
  if (!this->nfc_write_mifare_classic_block_(2, block_data))
    return false;
  block_data.assign(BLOCK_3_TRAILER.begin(), BLOCK_3_TRAILER.end());
  if (!this->nfc_write_mifare_classic_block_(3, block_data))
    return false;

  ESP_LOGD(TAG, "Sector 0 formatted to NDEF");

  for (int block = 4; block < 64; block += 4) {
    if (!this->nfc_auth_mifare_classic_block_(uid, block + 3, MIFARE_CMD_AUTH_B, DEFAULT_KEY)) {
      return false;
    }
    if (block == 4) {
      block_data.assign(EMPTY_NDEF_MESSAGE.begin(), EMPTY_NDEF_MESSAGE.end());
      if (!this->nfc_write_mifare_classic_block_(block, block_data)) {
        ESP_LOGE(TAG, "Unable to write block %d", block);
      }
    } else {
      block_data.assign(BLANK_BUFFER.begin(), BLANK_BUFFER.end());
      if (!this->nfc_write_mifare_classic_block_(block, block_data)) {
        ESP_LOGE(TAG, "Unable to write block %d", block);
      }
    }
    block_data.assign(BLANK_BUFFER.begin(), BLANK_BUFFER.end());
    if (!this->nfc_write_mifare_classic_block_(block + 1, block_data)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 1);
    }
    block_data.assign(BLANK_BUFFER.begin(), BLANK_BUFFER.end());
    if (!this->nfc_write_mifare_classic_block_(block + 2, block_data)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 2);
    }
    block_data.assign(NDEF_TRAILER.begin(), NDEF_TRAILER.end());
    if (!this->nfc_write_mifare_classic_block_(block + 3, block_data)) {
      ESP_LOGE(TAG, "Unable to write trailer block %d", block + 3);
    }
  }
  return true;
}

bool Nfcc::write_mifare_classic_tag_(NfcTagUid &uid, NdefMessage *message) {
  auto encoded = message->encode();

  uint32_t message_length = encoded.size();
  uint32_t buffer_length = get_mifare_classic_buffer_size(message_length);

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
  uint8_t current_block = 4;

  while (index < buffer_length) {
    if (mifare_classic_is_first_block(current_block)) {
      if (!this->nfc_auth_mifare_classic_block_(uid, current_block, MIFARE_CMD_AUTH_A, NDEF_KEY)) {
        return false;
      }
    }

    std::vector<uint8_t> block_data(encoded.begin() + index, encoded.begin() + index + MIFARE_CLASSIC_BLOCK_SIZE);
    if (!this->nfc_write_mifare_classic_block_(current_block, block_data)) {
      return false;
    }
    index += MIFARE_CLASSIC_BLOCK_SIZE;
    current_block++;

    if (mifare_classic_is_trailer_block(current_block)) {
      current_block++;
    }
  }

  return true;
}

}  // namespace nfc
}  // namespace esphome
