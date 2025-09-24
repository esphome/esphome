#include "nfc.h"
#include <cstdio>
#include "esphome/core/log.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc";

std::string format_uid(std::vector<uint8_t> &uid) {
  char buf[(uid.size() * 2) + uid.size() - 1];
  int offset = 0;
  for (size_t i = 0; i < uid.size(); i++) {
    const char *format = "%02X";
    if (i + 1 < uid.size())
      format = "%02X-";
    offset += sprintf(buf + offset, format, uid[i]);
  }
  return std::string(buf);
}

std::string format_bytes(std::vector<uint8_t> &bytes) {
  char buf[(bytes.size() * 2) + bytes.size() - 1];
  int offset = 0;
  for (size_t i = 0; i < bytes.size(); i++) {
    const char *format = "%02X";
    if (i + 1 < bytes.size())
      format = "%02X ";
    offset += sprintf(buf + offset, format, bytes[i]);
  }
  return std::string(buf);
}

uint8_t guess_tag_type(uint8_t uid_length) {
  if (uid_length == 4) {
    return TAG_TYPE_MIFARE_CLASSIC;
  } else {
    return TAG_TYPE_2;
  }
}

uint8_t get_mifare_classic_ndef_start_index(std::vector<uint8_t> &data) {
  for (uint8_t i = 0; i < MIFARE_CLASSIC_BLOCK_SIZE; i++) {
    if (data[i] == 0x00) {
      // Do nothing, skip
    } else if (data[i] == 0x03) {
      return i;
    } else {
      return -2;
    }
  }
  return -1;
}

bool decode_mifare_classic_tlv(std::vector<uint8_t> &data, uint32_t &message_length, uint8_t &message_start_index) {
  auto i = get_mifare_classic_ndef_start_index(data);
  if (data[i] != 0x03) {
    ESP_LOGE(TAG, "Error, Can't decode message length.");
    return false;
  }
  if (data[i + 1] == 0xFF) {
    message_length = ((0xFF & data[i + 2]) << 8) | (0xFF & data[i + 3]);
    message_start_index = i + MIFARE_CLASSIC_LONG_TLV_SIZE;
  } else {
    message_length = data[i + 1];
    message_start_index = i + MIFARE_CLASSIC_SHORT_TLV_SIZE;
  }
  return true;
}

uint32_t get_mifare_ultralight_buffer_size(uint32_t message_length) {
  uint32_t buffer_size = message_length + 2 + 1;
  if (buffer_size % MIFARE_ULTRALIGHT_READ_SIZE != 0)
    buffer_size = ((buffer_size / MIFARE_ULTRALIGHT_READ_SIZE) + 1) * MIFARE_ULTRALIGHT_READ_SIZE;
  return buffer_size;
}

uint32_t get_mifare_classic_buffer_size(uint32_t message_length) {
  uint32_t buffer_size = message_length;
  if (message_length < 255) {
    buffer_size += MIFARE_CLASSIC_SHORT_TLV_SIZE + 1;
  } else {
    buffer_size += MIFARE_CLASSIC_LONG_TLV_SIZE + 1;
  }
  if (buffer_size % MIFARE_CLASSIC_BLOCK_SIZE != 0) {
    buffer_size = ((buffer_size / MIFARE_CLASSIC_BLOCK_SIZE) + 1) * MIFARE_CLASSIC_BLOCK_SIZE;
  }
  return buffer_size;
}

bool mifare_classic_is_first_block(uint8_t block_num) {
  if (block_num < MIFARE_CLASSIC_BLOCKS_PER_SECT_LOW * MIFARE_CLASSIC_16BLOCK_SECT_START) {
    return (block_num % MIFARE_CLASSIC_BLOCKS_PER_SECT_LOW == 0);
  } else {
    return (block_num % MIFARE_CLASSIC_BLOCKS_PER_SECT_HIGH == 0);
  }
}

bool mifare_classic_is_trailer_block(uint8_t block_num) {
  if (block_num < MIFARE_CLASSIC_BLOCKS_PER_SECT_LOW * MIFARE_CLASSIC_16BLOCK_SECT_START) {
    return ((block_num + 1) % MIFARE_CLASSIC_BLOCKS_PER_SECT_LOW == 0);
  } else {
    return ((block_num + 1) % MIFARE_CLASSIC_BLOCKS_PER_SECT_HIGH == 0);
  }
}

}  // namespace nfc
}  // namespace esphome
