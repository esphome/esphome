#include "nfc.h"

#include <algorithm>
#include <memory>

#include "esphome/core/log.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.type4";

static constexpr uint8_t ISO_CLA = 0x00;
static constexpr uint8_t ISO_INS_SELECT = 0xA4;
static constexpr uint8_t ISO_INS_READ_BINARY = 0xB0;
static constexpr uint8_t ISO_INS_UPDATE_BINARY = 0xD6;
static constexpr uint8_t TYPE_4_NDEF_FILE_CONTROL_TLV = 0x04;
static constexpr uint16_t TYPE_4_CC_FILE_MIN_SIZE = 15;
static constexpr uint16_t TYPE_4_CC_FILE_MAX_SIZE = 64;
static constexpr uint16_t TYPE_4_MAX_NDEF_MESSAGE_SIZE = 1024;
static constexpr uint8_t TYPE_4_READ_CHUNK_SIZE = 48;
static constexpr uint8_t TYPE_4_WRITE_CHUNK_SIZE = 48;
static constexpr uint8_t NDEF_APPLICATION_DFN_V2[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
static constexpr uint8_t NDEF_APPLICATION_DFN_V1[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
static constexpr uint8_t CC_FILE_ID[2] = {0xE1, 0x03};

static bool parse_type_4_ndef_mapping(const uint8_t *cc_data, uint16_t cc_size, Type4NdefMapping &mapping) {
  if (cc_size < TYPE_4_CC_FILE_MIN_SIZE) {
    return false;
  }

  const uint16_t cc_length = (static_cast<uint16_t>(cc_data[0]) << 8) | cc_data[1];
  if (cc_length < TYPE_4_CC_FILE_MIN_SIZE || cc_length > cc_size) {
    return false;
  }

  mapping.max_read_size = (static_cast<uint16_t>(cc_data[3]) << 8) | cc_data[4];

  for (uint16_t index = 7; index + 1 < cc_length;) {
    const uint8_t tlv_type = cc_data[index++];
    if (tlv_type == 0x00) {
      continue;
    }

    const uint8_t tlv_length = cc_data[index++];
    if (index + tlv_length > cc_length) {
      return false;
    }

    if (tlv_type == TYPE_4_NDEF_FILE_CONTROL_TLV) {
      if (tlv_length != 6) {
        return false;
      }
      mapping.ndef_file_id = (static_cast<uint16_t>(cc_data[index]) << 8) | cc_data[index + 1];
      mapping.max_ndef_file_size = (static_cast<uint16_t>(cc_data[index + 2]) << 8) | cc_data[index + 3];
      mapping.write_access = cc_data[index + 5];
      return true;
    }

    index += tlv_length;
  }

  return false;
}

bool Nfcc::send_iso_dep_apdu_(const std::vector<uint8_t> &apdu, std::vector<uint8_t> &response) {
  if (!this->nfc_iso_dep_transceive_(apdu, response)) {
    return false;
  }
  if (response.size() < 2) {
    return false;
  }
  if (response[response.size() - 2] != 0x90 || response[response.size() - 1] != 0x00) {
    ESP_LOGW(TAG, "APDU failed: SW1=%02X SW2=%02X", response[response.size() - 2], response[response.size() - 1]);
    return false;
  }
  response.resize(response.size() - 2);
  return true;
}

std::unique_ptr<NfcTag> Nfcc::read_iso_dep_tag_(NfcTagUid &uid) {
  auto make_uid_only_tag = [&uid]() { return std::make_unique<NfcTag>(uid); };
  auto make_type_4_tag = [&uid]() { return std::make_unique<NfcTag>(uid, NFC_FORUM_TYPE_4); };

  auto select_file = [this](uint16_t file_id) {
    std::vector<uint8_t> apdu = {ISO_CLA,
                                 ISO_INS_SELECT,
                                 0x00,
                                 0x0C,
                                 0x02,
                                 static_cast<uint8_t>((file_id >> 8) & 0xFF),
                                 static_cast<uint8_t>(file_id & 0xFF)};
    std::vector<uint8_t> response;
    return this->send_iso_dep_apdu_(apdu, response);
  };

  auto read_binary = [this](uint16_t offset, uint8_t read_size, std::vector<uint8_t> &response) {
    std::vector<uint8_t> apdu = {ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((offset >> 8) & 0xFF),
                                 static_cast<uint8_t>(offset & 0xFF), read_size};
    return this->send_iso_dep_apdu_(apdu, response);
  };

  std::vector<uint8_t> response;
  std::vector<uint8_t> select_app_v2 = {ISO_CLA,
                                        ISO_INS_SELECT,
                                        0x04,
                                        0x00,
                                        0x07,
                                        NDEF_APPLICATION_DFN_V2[0],
                                        NDEF_APPLICATION_DFN_V2[1],
                                        NDEF_APPLICATION_DFN_V2[2],
                                        NDEF_APPLICATION_DFN_V2[3],
                                        NDEF_APPLICATION_DFN_V2[4],
                                        NDEF_APPLICATION_DFN_V2[5],
                                        NDEF_APPLICATION_DFN_V2[6]};
  if (!this->send_iso_dep_apdu_(select_app_v2, response)) {
    std::vector<uint8_t> select_app_v1 = {ISO_CLA,
                                          ISO_INS_SELECT,
                                          0x04,
                                          0x00,
                                          0x07,
                                          NDEF_APPLICATION_DFN_V1[0],
                                          NDEF_APPLICATION_DFN_V1[1],
                                          NDEF_APPLICATION_DFN_V1[2],
                                          NDEF_APPLICATION_DFN_V1[3],
                                          NDEF_APPLICATION_DFN_V1[4],
                                          NDEF_APPLICATION_DFN_V1[5],
                                          NDEF_APPLICATION_DFN_V1[6]};
    if (!this->send_iso_dep_apdu_(select_app_v1, response)) {
      ESP_LOGD(TAG, "NDEF application select failed");
      return make_uid_only_tag();
    }
  }

  if (!select_file((static_cast<uint16_t>(CC_FILE_ID[0]) << 8) | CC_FILE_ID[1])) {
    ESP_LOGD(TAG, "Capability container select failed");
    return make_uid_only_tag();
  }

  std::vector<uint8_t> cc_header;
  if (!read_binary(0, 2, cc_header) || cc_header.size() != 2) {
    ESP_LOGD(TAG, "Capability container header read failed");
    return make_uid_only_tag();
  }

  const uint16_t cc_size = (static_cast<uint16_t>(cc_header[0]) << 8) | cc_header[1];
  if (cc_size < TYPE_4_CC_FILE_MIN_SIZE || cc_size > TYPE_4_CC_FILE_MAX_SIZE) {
    ESP_LOGD(TAG, "Unsupported capability container size: %u", cc_size);
    return make_uid_only_tag();
  }

  std::vector<uint8_t> cc_data;
  cc_data.reserve(cc_size);
  uint16_t cc_offset = 0;
  while (cc_offset < cc_size) {
    std::vector<uint8_t> chunk;
    uint8_t chunk_size = std::min<uint16_t>(cc_size - cc_offset, TYPE_4_READ_CHUNK_SIZE);
    if (!read_binary(cc_offset, chunk_size, chunk) || chunk.size() != chunk_size) {
      ESP_LOGD(TAG, "Capability container read failed at offset %u", cc_offset);
      return make_uid_only_tag();
    }
    cc_data.insert(cc_data.end(), chunk.begin(), chunk.end());
    cc_offset += chunk_size;
  }

  Type4NdefMapping mapping{};
  if (!parse_type_4_ndef_mapping(cc_data.data(), cc_data.size(), mapping)) {
    ESP_LOGD(TAG, "No readable Type 4 NDEF mapping found");
    return make_uid_only_tag();
  }

  auto type_4_tag = make_type_4_tag();

  if (!select_file(mapping.ndef_file_id)) {
    ESP_LOGD(TAG, "NDEF file select failed");
    return type_4_tag;
  }

  std::vector<uint8_t> nlen_resp;
  if (!read_binary(0, 2, nlen_resp) || nlen_resp.size() < 2) {
    ESP_LOGD(TAG, "NLEN read failed");
    return type_4_tag;
  }

  uint16_t ndef_length = (static_cast<uint16_t>(nlen_resp[0]) << 8) | nlen_resp[1];
  if (ndef_length == 0) {
    return type_4_tag;
  }

  if (ndef_length + 2 > mapping.max_ndef_file_size || ndef_length > TYPE_4_MAX_NDEF_MESSAGE_SIZE) {
    ESP_LOGW(TAG, "NDEF length %u exceeds supported file size", ndef_length);
    return type_4_tag;
  }

  const uint16_t read_chunk_size = std::min<uint16_t>(TYPE_4_READ_CHUNK_SIZE, mapping.max_read_size);
  if (read_chunk_size == 0) {
    ESP_LOGD(TAG, "Capability container reported zero read size");
    return type_4_tag;
  }

  std::vector<uint8_t> ndef_data;
  ndef_data.reserve(ndef_length);
  uint16_t offset = 0;
  while (offset < ndef_length) {
    std::vector<uint8_t> chunk;
    uint8_t chunk_size = std::min<uint16_t>(ndef_length - offset, read_chunk_size);
    if (!read_binary(2 + offset, chunk_size, chunk) || chunk.size() != chunk_size) {
      ESP_LOGW(TAG, "NDEF data read failed at offset %u", offset);
      return type_4_tag;
    }
    ndef_data.insert(ndef_data.end(), chunk.begin(), chunk.end());
    offset += chunk_size;
  }

  return std::make_unique<NfcTag>(uid, NFC_FORUM_TYPE_4, ndef_data);
}

bool Nfcc::clean_iso_dep_tag_() {
  std::vector<uint8_t> response;
  std::vector<uint8_t> select_app_v2 = {ISO_CLA,
                                        ISO_INS_SELECT,
                                        0x04,
                                        0x00,
                                        0x07,
                                        NDEF_APPLICATION_DFN_V2[0],
                                        NDEF_APPLICATION_DFN_V2[1],
                                        NDEF_APPLICATION_DFN_V2[2],
                                        NDEF_APPLICATION_DFN_V2[3],
                                        NDEF_APPLICATION_DFN_V2[4],
                                        NDEF_APPLICATION_DFN_V2[5],
                                        NDEF_APPLICATION_DFN_V2[6]};
  if (!this->send_iso_dep_apdu_(select_app_v2, response)) {
    std::vector<uint8_t> select_app_v1 = {ISO_CLA,
                                          ISO_INS_SELECT,
                                          0x04,
                                          0x00,
                                          0x07,
                                          NDEF_APPLICATION_DFN_V1[0],
                                          NDEF_APPLICATION_DFN_V1[1],
                                          NDEF_APPLICATION_DFN_V1[2],
                                          NDEF_APPLICATION_DFN_V1[3],
                                          NDEF_APPLICATION_DFN_V1[4],
                                          NDEF_APPLICATION_DFN_V1[5],
                                          NDEF_APPLICATION_DFN_V1[6]};
    if (!this->send_iso_dep_apdu_(select_app_v1, response)) {
      return false;
    }
  }

  if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02, CC_FILE_ID[0], CC_FILE_ID[1]}, response)) {
    return false;
  }
  std::vector<uint8_t> cc_header;
  if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_READ_BINARY, 0x00, 0x00, 0x02}, cc_header) || cc_header.size() != 2) {
    return false;
  }

  uint16_t cc_size = (static_cast<uint16_t>(cc_header[0]) << 8) | cc_header[1];
  if (cc_size < TYPE_4_CC_FILE_MIN_SIZE || cc_size > TYPE_4_CC_FILE_MAX_SIZE) {
    return false;
  }

  std::vector<uint8_t> cc_data;
  cc_data.reserve(cc_size);
  uint16_t cc_offset = 0;
  while (cc_offset < cc_size) {
    std::vector<uint8_t> chunk;
    uint8_t chunk_size = std::min<uint16_t>(cc_size - cc_offset, TYPE_4_READ_CHUNK_SIZE);
    if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((cc_offset >> 8) & 0xFF),
                                   static_cast<uint8_t>(cc_offset & 0xFF), chunk_size},
                                  chunk) ||
        chunk.size() != chunk_size) {
      return false;
    }
    cc_data.insert(cc_data.end(), chunk.begin(), chunk.end());
    cc_offset += chunk_size;
  }

  Type4NdefMapping mapping{};
  if (!parse_type_4_ndef_mapping(cc_data.data(), cc_data.size(), mapping) || mapping.write_access != 0x00) {
    return false;
  }

  if (!this->send_iso_dep_apdu_(
          {ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02, static_cast<uint8_t>((mapping.ndef_file_id >> 8) & 0xFF),
           static_cast<uint8_t>(mapping.ndef_file_id & 0xFF)},
          response)) {
    return false;
  }

  std::vector<uint8_t> clear_nlen = {ISO_CLA, ISO_INS_UPDATE_BINARY, 0x00, 0x00, 0x02, 0x00, 0x00};
  return this->send_iso_dep_apdu_(clear_nlen, response);
}

bool Nfcc::write_iso_dep_tag_(NdefMessage *message) {
  std::vector<uint8_t> response;
  std::vector<uint8_t> select_app_v2 = {ISO_CLA,
                                        ISO_INS_SELECT,
                                        0x04,
                                        0x00,
                                        0x07,
                                        NDEF_APPLICATION_DFN_V2[0],
                                        NDEF_APPLICATION_DFN_V2[1],
                                        NDEF_APPLICATION_DFN_V2[2],
                                        NDEF_APPLICATION_DFN_V2[3],
                                        NDEF_APPLICATION_DFN_V2[4],
                                        NDEF_APPLICATION_DFN_V2[5],
                                        NDEF_APPLICATION_DFN_V2[6]};
  if (!this->send_iso_dep_apdu_(select_app_v2, response)) {
    std::vector<uint8_t> select_app_v1 = {ISO_CLA,
                                          ISO_INS_SELECT,
                                          0x04,
                                          0x00,
                                          0x07,
                                          NDEF_APPLICATION_DFN_V1[0],
                                          NDEF_APPLICATION_DFN_V1[1],
                                          NDEF_APPLICATION_DFN_V1[2],
                                          NDEF_APPLICATION_DFN_V1[3],
                                          NDEF_APPLICATION_DFN_V1[4],
                                          NDEF_APPLICATION_DFN_V1[5],
                                          NDEF_APPLICATION_DFN_V1[6]};
    if (!this->send_iso_dep_apdu_(select_app_v1, response)) {
      return false;
    }
  }

  if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02, CC_FILE_ID[0], CC_FILE_ID[1]}, response)) {
    return false;
  }

  std::vector<uint8_t> cc_header;
  if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_READ_BINARY, 0x00, 0x00, 0x02}, cc_header) || cc_header.size() != 2) {
    return false;
  }

  uint16_t cc_size = (static_cast<uint16_t>(cc_header[0]) << 8) | cc_header[1];
  if (cc_size < TYPE_4_CC_FILE_MIN_SIZE || cc_size > TYPE_4_CC_FILE_MAX_SIZE) {
    return false;
  }

  std::vector<uint8_t> cc_data;
  cc_data.reserve(cc_size);
  uint16_t cc_offset = 0;
  while (cc_offset < cc_size) {
    std::vector<uint8_t> chunk;
    uint8_t chunk_size = std::min<uint16_t>(cc_size - cc_offset, TYPE_4_READ_CHUNK_SIZE);
    if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((cc_offset >> 8) & 0xFF),
                                   static_cast<uint8_t>(cc_offset & 0xFF), chunk_size},
                                  chunk) ||
        chunk.size() != chunk_size) {
      return false;
    }
    cc_data.insert(cc_data.end(), chunk.begin(), chunk.end());
    cc_offset += chunk_size;
  }

  Type4NdefMapping mapping{};
  if (!parse_type_4_ndef_mapping(cc_data.data(), cc_data.size(), mapping) || mapping.write_access != 0x00) {
    return false;
  }

  auto encoded = message->encode();
  if (encoded.size() + 2 > mapping.max_ndef_file_size || encoded.size() > TYPE_4_MAX_NDEF_MESSAGE_SIZE) {
    return false;
  }

  if (!this->send_iso_dep_apdu_(
          {ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02, static_cast<uint8_t>((mapping.ndef_file_id >> 8) & 0xFF),
           static_cast<uint8_t>(mapping.ndef_file_id & 0xFF)},
          response)) {
    return false;
  }

  if (!this->send_iso_dep_apdu_({ISO_CLA, ISO_INS_UPDATE_BINARY, 0x00, 0x00, 0x02, 0x00, 0x00}, response)) {
    return false;
  }

  uint16_t offset = 0;
  while (offset < encoded.size()) {
    uint8_t chunk_size = std::min<uint16_t>(encoded.size() - offset, TYPE_4_WRITE_CHUNK_SIZE);
    std::vector<uint8_t> apdu = {ISO_CLA, ISO_INS_UPDATE_BINARY, static_cast<uint8_t>(((2 + offset) >> 8) & 0xFF),
                                 static_cast<uint8_t>((2 + offset) & 0xFF), chunk_size};
    apdu.insert(apdu.end(), encoded.begin() + offset, encoded.begin() + offset + chunk_size);
    if (!this->send_iso_dep_apdu_(apdu, response)) {
      return false;
    }
    offset += chunk_size;
  }

  uint16_t nlen = encoded.size();
  std::vector<uint8_t> nlen_apdu = {ISO_CLA,
                                    ISO_INS_UPDATE_BINARY,
                                    0x00,
                                    0x00,
                                    0x02,
                                    static_cast<uint8_t>((nlen >> 8) & 0xFF),
                                    static_cast<uint8_t>(nlen & 0xFF)};
  return this->send_iso_dep_apdu_(nlen_apdu, response);
}

}  // namespace nfc
}  // namespace esphome
