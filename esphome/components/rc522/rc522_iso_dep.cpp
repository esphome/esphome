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

static constexpr uint16_t ISO_DEP_TRANSCEIVE_TIMEOUT_MS = 100;
static constexpr uint16_t RC522_CRC_TIMEOUT_MS = 500;

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
  uint8_t valid_bits = 0;
  StatusCode status = this->pcd_transceive_sync_raw_(send_data, send_len, recv_data, recv_len, valid_bits);
  if (status != STATUS_OK) {
    return status;
  }
  if (valid_bits != 0) {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

RC522::StatusCode RC522::pcd_transceive_sync_raw_(const uint8_t *send_data, uint8_t send_len, uint8_t *recv_data,
                                                  uint8_t &recv_len, uint8_t &valid_bits) {
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

  valid_bits = this->pcd_read_register(CONTROL_REG) & 0x07;

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

void RC522::pcd_stop_crypto1_() { this->pcd_clear_register_bit_mask_(STATUS2_REG, 0x08); }

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

bool RC522::nfc_iso_dep_transceive_(const std::vector<uint8_t> &send, std::vector<uint8_t> &response) {
  uint8_t recv_data[RC522_FIFO_SIZE];
  uint8_t recv_len = sizeof(recv_data);
  StatusCode status = this->iso_dep_transceive_(send.data(), send.size(), recv_data, recv_len);
  if (status != STATUS_OK) {
    response.clear();
    return false;
  }
  response.assign(recv_data, recv_data + recv_len);
  return true;
}

std::unique_ptr<nfc::NfcTag> RC522::read_iso_dep_tag_(nfc::NfcTagUid &uid) {
  if (this->rats_() != STATUS_OK) {
    ESP_LOGD(TAG, "RATS failed, returning UID-only tag");
    return make_unique<nfc::NfcTag>(uid);
  }
  return this->Nfcc::read_iso_dep_tag_(uid);
}

}  // namespace rc522
}  // namespace esphome
