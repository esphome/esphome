#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "ndef_message.h"
#include "ndef_record.h"
#include "nfc_tag.h"

#include <vector>

namespace esphome {
namespace nfc {

static const uint8_t MIFARE_CLASSIC_BLOCK_SIZE = 16;
static const uint8_t MIFARE_CLASSIC_LONG_TLV_SIZE = 4;
static const uint8_t MIFARE_CLASSIC_SHORT_TLV_SIZE = 2;
static const uint8_t MIFARE_CLASSIC_BLOCKS_PER_SECT_LOW = 4;
static const uint8_t MIFARE_CLASSIC_BLOCKS_PER_SECT_HIGH = 16;
static const uint8_t MIFARE_CLASSIC_16BLOCK_SECT_START = 32;

static const uint8_t MIFARE_ULTRALIGHT_PAGE_SIZE = 4;
static const uint8_t MIFARE_ULTRALIGHT_READ_SIZE = 4;
static const uint8_t MIFARE_ULTRALIGHT_DATA_START_PAGE = 4;
static const uint8_t MIFARE_ULTRALIGHT_MAX_PAGE = 63;

static const uint8_t TAG_TYPE_MIFARE_CLASSIC = 0;
static const uint8_t TAG_TYPE_1 = 1;
static const uint8_t TAG_TYPE_2 = 2;
static const uint8_t TAG_TYPE_3 = 3;
static const uint8_t TAG_TYPE_4 = 4;
static const uint8_t TAG_TYPE_UNKNOWN = 99;

// Mifare Commands
static const uint8_t MIFARE_CMD_AUTH_A = 0x60;
static const uint8_t MIFARE_CMD_AUTH_B = 0x61;
static const uint8_t MIFARE_CMD_HALT = 0x50;
static const uint8_t MIFARE_CMD_READ = 0x30;
static const uint8_t MIFARE_CMD_WRITE = 0xA0;
static const uint8_t MIFARE_CMD_WRITE_ULTRALIGHT = 0xA2;

// Mifare Ack/Nak
static const uint8_t MIFARE_CMD_ACK = 0x0A;
static const uint8_t MIFARE_CMD_NAK_INVALID_XFER_BUFF_VALID = 0x00;
static const uint8_t MIFARE_CMD_NAK_CRC_ERROR_XFER_BUFF_VALID = 0x01;
static const uint8_t MIFARE_CMD_NAK_INVALID_XFER_BUFF_INVALID = 0x04;
static const uint8_t MIFARE_CMD_NAK_CRC_ERROR_XFER_BUFF_INVALID = 0x05;

static const char *const MIFARE_CLASSIC = "Mifare Classic";
static const char *const NFC_FORUM_TYPE_2 = "NFC Forum Type 2";
static const char *const ERROR = "Error";

static const uint8_t DEFAULT_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t NDEF_KEY[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
static const uint8_t MAD_KEY[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};

/// Max UID size is 10 bytes, formatted as "XX-XX-XX-XX-XX-XX-XX-XX-XX-XX\0" = 30 chars
static constexpr size_t FORMAT_UID_BUFFER_SIZE = 30;
/// Format UID to buffer with '-' separator (e.g., "04-11-22-33"). Returns buffer for inline use.
char *format_uid_to(char *buffer, const std::vector<uint8_t> &uid);

/// Buffer size for format_bytes_to (64 bytes max = 192 chars with space separator)
static constexpr size_t FORMAT_BYTES_BUFFER_SIZE = 192;
/// Format bytes to buffer with ' ' separator (e.g., "04 11 22 33"). Returns buffer for inline use.
char *format_bytes_to(char *buffer, const std::vector<uint8_t> &bytes);

// Remove before 2026.6.0
ESPDEPRECATED("Use format_uid_to() with stack buffer instead. Removed in 2026.6.0", "2025.12.0")
std::string format_uid(const std::vector<uint8_t> &uid);
// Remove before 2026.6.0
ESPDEPRECATED("Use format_bytes_to() with stack buffer instead. Removed in 2026.6.0", "2025.12.0")
std::string format_bytes(const std::vector<uint8_t> &bytes);

uint8_t guess_tag_type(uint8_t uid_length);
uint8_t get_mifare_classic_ndef_start_index(std::vector<uint8_t> &data);
bool decode_mifare_classic_tlv(std::vector<uint8_t> &data, uint32_t &message_length, uint8_t &message_start_index);
uint32_t get_mifare_classic_buffer_size(uint32_t message_length);

bool mifare_classic_is_first_block(uint8_t block_num);
bool mifare_classic_is_trailer_block(uint8_t block_num);

uint32_t get_mifare_ultralight_buffer_size(uint32_t message_length);

class NfcTagListener {
 public:
  virtual void tag_off(NfcTag &tag) {}
  virtual void tag_on(NfcTag &tag) {}
};

class Nfcc {
 public:
  void register_listener(NfcTagListener *listener) { this->tag_listeners_.push_back(listener); }

 protected:
  std::vector<NfcTagListener *> tag_listeners_;
};

}  // namespace nfc
}  // namespace esphome
