#include <algorithm>
#include <array>
#include <cinttypes>
#include <memory>

#include "pn532.h"
#include "esphome/core/log.h"

namespace esphome::pn532 {

static const char *const TAG = "pn532.mifare_classic";

std::unique_ptr<nfc::NfcTag> PN532::read_mifare_classic_tag_(nfc::NfcTagUid &uid) {
  uint8_t current_block = 4;
  uint8_t message_start_index = 0;
  uint32_t message_length = 0;

  std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> block_data;
  if (this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY)) {
    if (this->read_mifare_classic_block_(current_block, block_data)) {
      if (!nfc::decode_mifare_classic_tlv(block_data, message_length, message_start_index)) {
        return make_unique<nfc::NfcTag>(uid, nfc::ERROR);
      }
    } else {
      ESP_LOGE(TAG, "Failed to read block %d", current_block);
      return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
    }
  } else {
    ESP_LOGV(TAG, "Tag is not NDEF formatted");
    return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
  }
  if (message_length > MIFARE_CLASSIC_MAX_NDEF_SIZE) {
    ESP_LOGE(TAG, "NDEF message too long: %" PRIu32 " bytes", message_length);
    return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
  }

  const uint32_t buffer_size = nfc::get_mifare_classic_buffer_size(message_length);
  FixedVector<uint8_t> buffer;
  buffer.init(buffer_size);

  while (buffer.size() < buffer_size) {
    if (nfc::mifare_classic_is_first_block(current_block)) {
      if (!this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY)) {
        ESP_LOGE(TAG, "Error, Block authentication failed for %d", current_block);
        return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
      }
    }
    if (!this->read_mifare_classic_block_(current_block, block_data)) {
      ESP_LOGE(TAG, "Error reading block %d", current_block);
      return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
    }
    for (const uint8_t byte : block_data) {
      buffer.push_back(byte);
    }

    current_block++;
    if (nfc::mifare_classic_is_trailer_block(current_block)) {
      current_block++;
    }
  }

  if (message_start_index >= buffer.size()) {
    return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
  }

  return make_unique<nfc::NfcTag>(
      uid, nfc::MIFARE_CLASSIC,
      make_unique<nfc::NdefMessage>(std::span<const uint8_t>(buffer).subspan(message_start_index)));
}

bool PN532::read_mifare_classic_block_(uint8_t block_num, std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> &data) {
  PN532Frame response;
  if (!this->in_data_exchange_(
          {
              PN532_COMMAND_INDATAEXCHANGE,
              0x01,  // One card
              nfc::MIFARE_CMD_READ,
              block_num,
          },
          response) ||
      response.size() != nfc::MIFARE_CLASSIC_BLOCK_SIZE) {
    return false;
  }
  std::copy(response.begin(), response.end(), data.begin());

  char data_buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
  ESP_LOGVV(TAG, " Block %d: %s", block_num, nfc::format_bytes_to(data_buf, data));
  return true;
}

bool PN532::auth_mifare_classic_block_(nfc::NfcTagUid &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key) {
  // InDataExchange, Tg, key slot, block, key (6), UID (4)
  StaticVector<uint8_t, 14> data = {
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,       // One card
      key_num,    // Mifare Key slot
      block_num,  // Block number
  };
  for (size_t i = 0; i < 6; i++) {
    data.push_back(key[i]);
  }
  // the command takes exactly 4 UID bytes (UM0701-02, 7.3.8); for 7-byte UIDs these are the last 4, as in libnfc
  if (uid.size() < 4) {
    return false;
  }
  for (size_t i = uid.size() - 4; i < uid.size(); i++) {
    data.push_back(uid[i]);
  }

  PN532Frame response;
  if (!this->in_data_exchange_(data, response)) {
    ESP_LOGE(TAG, "Authentication failed - Block 0x%02x", block_num);
    return false;
  }

  return true;
}

bool PN532::format_mifare_classic_mifare_(nfc::NfcTagUid &uid) {
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> BLANK_BUFFER = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> TRAILER_BUFFER = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x80, 0x69, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  bool error = false;

  for (int block = 0; block < 64; block += 4) {
    if (!this->auth_mifare_classic_block_(uid, block + 3, nfc::MIFARE_CMD_AUTH_B, nfc::DEFAULT_KEY)) {
      continue;
    }
    if (block != 0) {
      if (!this->write_mifare_classic_block_(block, BLANK_BUFFER)) {
        ESP_LOGE(TAG, "Unable to write block %d", block);
        error = true;
      }
    }
    if (!this->write_mifare_classic_block_(block + 1, BLANK_BUFFER)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 1);
      error = true;
    }
    if (!this->write_mifare_classic_block_(block + 2, BLANK_BUFFER)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 2);
      error = true;
    }
    if (!this->write_mifare_classic_block_(block + 3, TRAILER_BUFFER)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 3);
      error = true;
    }
  }

  return !error;
}

bool PN532::format_mifare_classic_ndef_(nfc::NfcTagUid &uid) {
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> EMPTY_NDEF_MESSAGE = {
      0x03, 0x03, 0xD0, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> BLANK_BLOCK = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> BLOCK_1_DATA = {
      0x14, 0x01, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> BLOCK_2_DATA = {
      0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> BLOCK_3_TRAILER = {
      0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x78, 0x77, 0x88, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  static constexpr std::array<uint8_t, nfc::MIFARE_CLASSIC_BLOCK_SIZE> NDEF_TRAILER = {
      0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07, 0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  if (!this->auth_mifare_classic_block_(uid, 0, nfc::MIFARE_CMD_AUTH_B, nfc::DEFAULT_KEY)) {
    ESP_LOGE(TAG, "Unable to authenticate block 0 for formatting!");
    return false;
  }
  if (!this->write_mifare_classic_block_(1, BLOCK_1_DATA))
    return false;
  if (!this->write_mifare_classic_block_(2, BLOCK_2_DATA))
    return false;
  if (!this->write_mifare_classic_block_(3, BLOCK_3_TRAILER))
    return false;

  ESP_LOGD(TAG, "Sector 0 formatted to NDEF");

  bool error = false;

  for (int block = 4; block < 64; block += 4) {
    if (!this->auth_mifare_classic_block_(uid, block + 3, nfc::MIFARE_CMD_AUTH_B, nfc::DEFAULT_KEY)) {
      return false;
    }
    if (block == 4) {
      if (!this->write_mifare_classic_block_(block, EMPTY_NDEF_MESSAGE)) {
        ESP_LOGE(TAG, "Unable to write block %d", block);
        error = true;
      }
    } else {
      if (!this->write_mifare_classic_block_(block, BLANK_BLOCK)) {
        ESP_LOGE(TAG, "Unable to write block %d", block);
        error = true;
      }
    }
    if (!this->write_mifare_classic_block_(block + 1, BLANK_BLOCK)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 1);
      error = true;
    }
    if (!this->write_mifare_classic_block_(block + 2, BLANK_BLOCK)) {
      ESP_LOGE(TAG, "Unable to write block %d", block + 2);
      error = true;
    }
    if (!this->write_mifare_classic_block_(block + 3, NDEF_TRAILER)) {
      ESP_LOGE(TAG, "Unable to write trailer block %d", block + 3);
      error = true;
    }
  }
  return !error;
}

bool PN532::write_mifare_classic_block_(uint8_t block_num, const std::span<const uint8_t> data) {
  StaticVector<uint8_t, 4 + nfc::MIFARE_CLASSIC_BLOCK_SIZE> cmd = {
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE,
      block_num,
  };
  for (const uint8_t byte : data) {
    cmd.push_back(byte);
  }

  PN532Frame response;
  if (!this->in_data_exchange_(cmd, response)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  return true;
}

bool PN532::write_mifare_classic_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  const auto encoded = message->encode();
  const uint32_t buffer_length = nfc::get_mifare_classic_buffer_size(encoded.size());
  FixedVector<uint8_t> buffer;
  nfc::fill_ndef_tlv(encoded, buffer_length, buffer);

  uint32_t index = 0;
  uint8_t current_block = 4;

  while (index < buffer_length) {
    if (nfc::mifare_classic_is_first_block(current_block)) {
      if (!this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY)) {
        return false;
      }
    }

    if (!this->write_mifare_classic_block_(current_block,
                                           std::span<const uint8_t>(&buffer[index], nfc::MIFARE_CLASSIC_BLOCK_SIZE))) {
      return false;
    }
    index += nfc::MIFARE_CLASSIC_BLOCK_SIZE;
    current_block++;

    if (nfc::mifare_classic_is_trailer_block(current_block)) {
      // Skipping as cannot write to trailer
      current_block++;
    }
  }
  return true;
}

}  // namespace esphome::pn532
