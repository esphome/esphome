#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "ndef_record.h"
#include "ndef_message.h"
#include "nfc_tag.h"

namespace esphome {
namespace nfc {

static const uint8_t BLOCK_SIZE = 16;
static const uint8_t LONG_TLV_SIZE = 4;
static const uint8_t SHORT_TLV_SIZE = 2;

static const uint8_t TAG_TYPE_MIFARE_CLASSIC = 0;
static const uint8_t TAG_TYPE_1 = 1;
static const uint8_t TAG_TYPE_2 = 2;
static const uint8_t TAG_TYPE_3 = 3;
static const uint8_t TAG_TYPE_4 = 4;
static const uint8_t TAG_TYPE_UNKNOWN = 99;

// Mifare Commands
static const uint8_t MIFARE_CMD_AUTH_A = 0x60;
static const uint8_t MIFARE_CMD_AUTH_B = 0x61;
static const uint8_t MIFARE_CMD_READ = 0x30;
static const uint8_t MIFARE_CMD_WRITE = 0xA0;

static const char *MIFARE_CLASSIC = "Mifare Classic";
static const char *ERROR = "Error";

static const uint8_t DEFAULT_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t NDEF_KEY[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};

std::string format_uid(std::vector<uint8_t> &uid);

uint8_t guess_tag_type(uint8_t uid_length);
uint8_t get_ndef_start_index(std::vector<uint8_t> &data);
bool decode_mifare_classic_tlv(std::vector<uint8_t> &data, uint32_t &message_length, uint8_t &message_start_index);
uint32_t get_buffer_size(uint32_t message_length);

bool mifare_classic_is_first_block(uint8_t block_num);
bool mifare_classic_is_trailer_block(uint8_t block_num);

}  // namespace nfc
}  // namespace esphome
