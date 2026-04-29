#include "rc522.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace rc522 {

static const char *const TAG = "rc522.iso_dep";

static constexpr uint8_t RC522_FIFO_SIZE = 64;
static constexpr uint8_t RC522_ISO_DEP_MAX_RETRIES = 6;

static constexpr uint8_t ISO_DEP_PCB_IBLOCK = 0x02;
static constexpr uint8_t ISO_DEP_PCB_SBLOCK_WTX = 0xF2;

static constexpr uint8_t ISO_CLA = 0x00;
static constexpr uint8_t ISO_INS_SELECT = 0xA4;
static constexpr uint8_t ISO_INS_READ_BINARY = 0xB0;

static constexpr uint8_t NDEF_APPLICATION_DFN[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
static constexpr uint8_t CC_FILE_ID[2] = {0xE1, 0x03};

static constexpr uint16_t ISO_DEP_TRANSCEIVE_TIMEOUT_MS = 100;
static constexpr uint16_t RC522_CRC_TIMEOUT_MS = 500;
static constexpr uint8_t TYPE_4_NDEF_FILE_CONTROL_TLV = 0x04;
static constexpr uint16_t TYPE_4_CC_FILE_MIN_SIZE = 15;
static constexpr uint16_t TYPE_4_CC_FILE_MAX_SIZE = 64;
static constexpr uint16_t TYPE_4_MAX_NDEF_MESSAGE_SIZE = 1024;
static constexpr uint8_t TYPE_4_READ_CHUNK_SIZE = 48;

struct Type4NdefMapping {
  uint16_t ndef_file_id;
  uint16_t max_ndef_file_size;
  uint16_t max_read_size;
};

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
      if (tlv_length != 6 || cc_data[index + 4] != 0x00) {
        return false;
      }

      mapping.ndef_file_id = (static_cast<uint16_t>(cc_data[index]) << 8) | cc_data[index + 1];
      mapping.max_ndef_file_size = (static_cast<uint16_t>(cc_data[index + 2]) << 8) | cc_data[index + 3];
      return true;
    }

    index += tlv_length;
  }

  return false;
}

void RC522::pcd_calculate_crc_sync_(uint8_t *data, uint8_t length, uint8_t *crc_out) {
  this->pcd_write_register(COMMAND_REG, PCD_IDLE);
  this->pcd_write_register(DIV_IRQ_REG, 0x04);
  this->pcd_write_register(FIFO_LEVEL_REG, 0x80);
  this->pcd_write_register(FIFO_DATA_REG, length, data);
  this->pcd_write_register(COMMAND_REG, PCD_CALC_CRC);

  uint32_t start = millis();
  while (millis() - start < RC522_CRC_TIMEOUT_MS) {
    uint8_t irq = this->pcd_read_register(DIV_IRQ_REG);
    if ((irq & 0x04) != 0) {
      this->pcd_write_register(COMMAND_REG, PCD_IDLE);
      crc_out[0] = this->pcd_read_register(CRC_RESULT_REG_L);
      crc_out[1] = this->pcd_read_register(CRC_RESULT_REG_H);
      return;
    }
    delayMicroseconds(100);
  }

  this->pcd_write_register(COMMAND_REG, PCD_IDLE);
  crc_out[0] = 0;
  crc_out[1] = 0;
}

RC522::StatusCode RC522::pcd_transceive_sync_(const uint8_t *send_data, uint8_t send_len, uint8_t *recv_data,
                                              uint8_t &recv_len) {
  delayMicroseconds(2000);

  this->pcd_write_register(COMMAND_REG, PCD_IDLE);
  this->pcd_write_register(COM_IRQ_REG, 0x7F);
  this->pcd_write_register(FIFO_LEVEL_REG, 0x80);

  uint8_t writable_len = send_len;
  if (writable_len > RC522_FIFO_SIZE) {
    return STATUS_NO_ROOM;
  }
  this->pcd_write_register(FIFO_DATA_REG, writable_len, const_cast<uint8_t *>(send_data));
  this->pcd_write_register(BIT_FRAMING_REG, 0x00);
  this->pcd_write_register(COMMAND_REG, PCD_TRANSCEIVE);
  this->pcd_set_register_bit_mask_(BIT_FRAMING_REG, 0x80);

  bool complete = false;
  uint32_t start = millis();
  while (millis() - start < ISO_DEP_TRANSCEIVE_TIMEOUT_MS) {
    uint8_t irq = this->pcd_read_register(COM_IRQ_REG);
    if ((irq & 0x01) != 0) {
      this->pcd_write_register(COMMAND_REG, PCD_IDLE);
      return STATUS_TIMEOUT;
    }
    if ((irq & 0x30) != 0) {
      complete = true;
      break;
    }
    delayMicroseconds(200);
  }

  if (!complete) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_TIMEOUT;
  }

  uint8_t error_reg = this->pcd_read_register(ERROR_REG);
  if ((error_reg & 0x13) != 0) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_ERROR;
  }

  uint8_t valid_bits = this->pcd_read_register(CONTROL_REG) & 0x07;
  if (valid_bits != 0) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_ERROR;
  }

  uint8_t fifo_len = this->pcd_read_register(FIFO_LEVEL_REG);
  if (fifo_len > RC522_FIFO_SIZE) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_NO_ROOM;
  }

  uint8_t fifo_data[RC522_FIFO_SIZE];
  this->pcd_read_register(FIFO_DATA_REG, fifo_len, fifo_data, 0);
  this->pcd_write_register(COMMAND_REG, PCD_IDLE);

  if (fifo_len > recv_len) {
    return STATUS_NO_ROOM;
  }

  memcpy(recv_data, fifo_data, fifo_len);
  recv_len = fifo_len;
  return STATUS_OK;
}

RC522::StatusCode RC522::rats_() {
  uint8_t cmd[4] = {PICC_CMD_RATS, 0x50, 0x00, 0x00};
  this->pcd_calculate_crc_sync_(cmd, 2, &cmd[2]);

  uint8_t response[RC522_FIFO_SIZE];
  uint8_t resp_len = sizeof(response);
  StatusCode status = this->pcd_transceive_sync_(cmd, 4, response, resp_len);

  if (status != STATUS_OK) {
    ESP_LOGW(TAG, "RATS failed: %d", status);
    return status;
  }

  if (resp_len < 4) {
    ESP_LOGW(TAG, "RATS response too short: %d bytes", resp_len);
    return STATUS_ERROR;
  }

  uint8_t crc_check[2];
  this->pcd_calculate_crc_sync_(response, resp_len - 2, crc_check);
  if (crc_check[0] != response[resp_len - 2] || crc_check[1] != response[resp_len - 1]) {
    ESP_LOGW(TAG, "RATS response CRC mismatch");
    return STATUS_CRC_WRONG;
  }

  return STATUS_OK;
}

RC522::StatusCode RC522::iso_dep_transceive_(const uint8_t *send_data, uint8_t send_len, uint8_t *recv_data,
                                             uint8_t &recv_len) {
  uint8_t frame[RC522_FIFO_SIZE];
  uint8_t frame_len = 0;

  uint8_t pcb = ISO_DEP_PCB_IBLOCK;

  for (uint8_t attempt = 0; attempt < RC522_ISO_DEP_MAX_RETRIES; ++attempt) {
    frame_len = 0;
    frame[frame_len++] = pcb;

    uint8_t data_len = std::min<uint8_t>(send_len, static_cast<uint8_t>(RC522_FIFO_SIZE - 3));
    if (data_len > 0 && send_data != nullptr) {
      memcpy(frame + frame_len, send_data, data_len);
      frame_len += data_len;
    }

    uint8_t crc[2];
    this->pcd_calculate_crc_sync_(frame, frame_len, crc);
    frame[frame_len++] = crc[0];
    frame[frame_len++] = crc[1];

    uint8_t response[RC522_FIFO_SIZE];
    uint8_t resp_len = sizeof(response);
    StatusCode status = this->pcd_transceive_sync_(frame, frame_len, response, resp_len);
    if (status != STATUS_OK) {
      return status;
    }

    if (resp_len < 3) {
      return STATUS_ERROR;
    }

    uint8_t crc_resp[2];
    this->pcd_calculate_crc_sync_(response, resp_len - 2, crc_resp);
    if (crc_resp[0] != response[resp_len - 2] || crc_resp[1] != response[resp_len - 1]) {
      ESP_LOGW(TAG, "ISO-DEP CRC mismatch");
      return STATUS_CRC_WRONG;
    }

    uint8_t data_start = 1;
    uint8_t data_end = resp_len - 2;

    uint8_t resp_pcb = response[0];
    if ((resp_pcb & 0xC0) == 0xC0 && (resp_pcb & 0x30) == 0x30 && (data_end - data_start) == 1) {
      pcb = ISO_DEP_PCB_SBLOCK_WTX;
      send_data = response + data_start;
      send_len = 1;
      continue;
    }

    if ((resp_pcb & 0xC0) == 0x80 && (resp_pcb & 0x20) != 0) {
      return STATUS_MIFARE_NACK;
    }

    uint8_t inf_len = data_end - data_start;
    if (inf_len > recv_len) {
      return STATUS_NO_ROOM;
    }

    memcpy(recv_data, response + data_start, inf_len);
    recv_len = inf_len;
    return STATUS_OK;
  }

  return STATUS_TIMEOUT;
}

RC522::StatusCode RC522::iso_dep_send_apdu_(const uint8_t *apdu, uint8_t apdu_len, uint8_t *resp, uint8_t &resp_len) {
  uint8_t response[RC522_FIFO_SIZE];
  uint8_t response_len = sizeof(response);

  StatusCode status = this->iso_dep_transceive_(apdu, apdu_len, response, response_len);
  if (status != STATUS_OK) {
    return status;
  }

  if (response_len < 2) {
    return STATUS_ERROR;
  }

  uint8_t sw1 = response[response_len - 2];
  uint8_t sw2 = response[response_len - 1];

  if (sw1 != 0x90 || sw2 != 0x00) {
    ESP_LOGW(TAG, "APDU failed: SW1=%02X SW2=%02X", sw1, sw2);
    return STATUS_ERROR;
  }

  uint8_t data_len = response_len - 2;
  if (data_len > resp_len) {
    return STATUS_NO_ROOM;
  }

  memcpy(resp, response, data_len);
  resp_len = data_len;
  return STATUS_OK;
}

std::unique_ptr<nfc::NfcTag> RC522::read_iso_dep_tag_(nfc::NfcTagUid &uid) {
  auto make_uid_only_tag = [&uid]() { return make_unique<nfc::NfcTag>(uid); };
  auto make_type_4_tag = [&uid]() { return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_4); };

  auto select_file = [this](uint16_t file_id) {
    uint8_t apdu[] = {ISO_CLA,
                      ISO_INS_SELECT,
                      0x00,
                      0x00,
                      0x02,
                      static_cast<uint8_t>((file_id >> 8) & 0xFF),
                      static_cast<uint8_t>(file_id & 0xFF),
                      0x00};
    uint8_t response[16];
    uint8_t response_len = sizeof(response);
    return this->iso_dep_send_apdu_(apdu, sizeof(apdu), response, response_len);
  };

  auto read_binary = [this](uint16_t offset, uint8_t read_size, uint8_t *response, uint8_t &response_len) {
    uint8_t apdu[5] = {ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((offset >> 8) & 0xFF),
                       static_cast<uint8_t>(offset & 0xFF), read_size};
    return this->iso_dep_send_apdu_(apdu, sizeof(apdu), response, response_len);
  };

  if (this->rats_() != STATUS_OK) {
    ESP_LOGD(TAG, "RATS failed, returning UID-only tag");
    return make_uid_only_tag();
  }

  uint8_t select_app[] = {ISO_CLA,
                          ISO_INS_SELECT,
                          0x04,
                          0x00,
                          0x07,
                          NDEF_APPLICATION_DFN[0],
                          NDEF_APPLICATION_DFN[1],
                          NDEF_APPLICATION_DFN[2],
                          NDEF_APPLICATION_DFN[3],
                          NDEF_APPLICATION_DFN[4],
                          NDEF_APPLICATION_DFN[5],
                          NDEF_APPLICATION_DFN[6],
                          0x00};
  uint8_t app_resp[16];
  uint8_t app_resp_len = sizeof(app_resp);
  if (this->iso_dep_send_apdu_(select_app, sizeof(select_app), app_resp, app_resp_len) != STATUS_OK) {
    ESP_LOGD(TAG, "NDEF application select failed");
    return make_uid_only_tag();
  }

  if (select_file((static_cast<uint16_t>(CC_FILE_ID[0]) << 8) | CC_FILE_ID[1]) != STATUS_OK) {
    ESP_LOGD(TAG, "Capability container select failed");
    return make_uid_only_tag();
  }

  uint8_t cc_header[2];
  uint8_t cc_header_len = sizeof(cc_header);
  if (read_binary(0, sizeof(cc_header), cc_header, cc_header_len) != STATUS_OK || cc_header_len != sizeof(cc_header)) {
    ESP_LOGD(TAG, "Capability container header read failed");
    return make_uid_only_tag();
  }

  const uint16_t cc_size = (static_cast<uint16_t>(cc_header[0]) << 8) | cc_header[1];
  if (cc_size < TYPE_4_CC_FILE_MIN_SIZE || cc_size > TYPE_4_CC_FILE_MAX_SIZE) {
    ESP_LOGD(TAG, "Unsupported capability container size: %u", cc_size);
    return make_uid_only_tag();
  }

  uint8_t cc_data[TYPE_4_CC_FILE_MAX_SIZE];
  uint16_t cc_offset = 0;
  while (cc_offset < cc_size) {
    uint8_t chunk = std::min<uint16_t>(cc_size - cc_offset, TYPE_4_READ_CHUNK_SIZE);
    uint8_t chunk_len = chunk;
    if (read_binary(cc_offset, chunk, cc_data + cc_offset, chunk_len) != STATUS_OK || chunk_len != chunk) {
      ESP_LOGD(TAG, "Capability container read failed at offset %u", cc_offset);
      return make_uid_only_tag();
    }
    cc_offset += chunk;
  }

  Type4NdefMapping mapping{};
  if (!parse_type_4_ndef_mapping(cc_data, cc_size, mapping)) {
    ESP_LOGD(TAG, "No readable Type 4 NDEF mapping found");
    return make_uid_only_tag();
  }

  auto type_4_tag = make_type_4_tag();

  if (select_file(mapping.ndef_file_id) != STATUS_OK) {
    ESP_LOGD(TAG, "NDEF file select failed");
    return type_4_tag;
  }

  uint8_t read_nlen[5] = {ISO_CLA, ISO_INS_READ_BINARY, 0x00, 0x00, 0x02};
  uint8_t nlen_resp[8];
  uint8_t nlen_resp_len = sizeof(nlen_resp);
  if (this->iso_dep_send_apdu_(read_nlen, 5, nlen_resp, nlen_resp_len) != STATUS_OK) {
    ESP_LOGD(TAG, "NLEN read failed");
    return type_4_tag;
  }

  if (nlen_resp_len < 2) {
    ESP_LOGD(TAG, "NLEN too short");
    return type_4_tag;
  }

  uint16_t ndef_length = (static_cast<uint16_t>(nlen_resp[0]) << 8) | nlen_resp[1];
  if (ndef_length == 0) {
    return type_4_tag;
  }

  if (ndef_length + 2 > mapping.max_ndef_file_size) {
    ESP_LOGW(TAG, "NDEF length %u exceeds file size %u", ndef_length, mapping.max_ndef_file_size);
    return type_4_tag;
  }

  if (ndef_length > TYPE_4_MAX_NDEF_MESSAGE_SIZE) {
    ESP_LOGW(TAG, "NDEF length %u exceeds supported size %u", ndef_length, TYPE_4_MAX_NDEF_MESSAGE_SIZE);
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
    uint8_t chunk = std::min<uint16_t>(ndef_length - offset, read_chunk_size);
    uint8_t read_apdu[5] = {ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>(((2 + offset) >> 8) & 0xFF),
                            static_cast<uint8_t>((2 + offset) & 0xFF), chunk};

    uint8_t chunk_resp[56];
    uint8_t chunk_resp_len = sizeof(chunk_resp);
    if (this->iso_dep_send_apdu_(read_apdu, 5, chunk_resp, chunk_resp_len) != STATUS_OK) {
      ESP_LOGW(TAG, "NDEF data read failed at offset %u", offset);
      return type_4_tag;
    }

    if (chunk_resp_len != chunk) {
      ESP_LOGW(TAG, "NDEF data truncated at offset %u", offset);
      return type_4_tag;
    }

    ndef_data.insert(ndef_data.end(), chunk_resp, chunk_resp + chunk_resp_len);
    offset += chunk;
  }

  return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_4, ndef_data);
}

}  // namespace rc522
}  // namespace esphome
