#include "rc522.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace rc522 {

static const char *const TAG = "rc522.mifare_ultralight";
static constexpr uint8_t RC522_MIFARE_READ_SIZE = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE * nfc::MIFARE_ULTRALIGHT_READ_SIZE;
static constexpr uint8_t RC522_WAIT_I_RQ = 0x30;
static constexpr uint32_t RC522_MIFARE_OPERATION_TIMEOUT_MS = 500;
static constexpr uint16_t RC522_MIFARE_TRANSCEIVE_DELAY_US = 2000;

static bool rc522_is_mifare_ack_(const uint8_t *buffer, uint8_t buffer_length, uint8_t valid_bits) {
  return buffer_length == 1 && valid_bits == 4 && (buffer[0] & 0x0F) == nfc::MIFARE_CMD_ACK;
}

std::unique_ptr<nfc::NfcTag> RC522::read_mifare_ultralight_tag_(nfc::NfcTagUid &uid) {
  return this->nfc::Nfcc::read_mifare_ultralight_tag_(uid);
}

bool RC522::nfc_read_mifare_ultralight_page_(uint8_t page, std::vector<uint8_t> &data) {
  data.resize(RC522_MIFARE_READ_SIZE);
  StatusCode status = this->read_mifare_ultralight_page_(page, data.data());
  if (status != STATUS_OK) {
    data.clear();
    return false;
  }
  return true;
}

RC522::StatusCode RC522::read_mifare_ultralight_page_(uint8_t page, uint8_t *data) {
  this->ndef_buffer_[0] = PICC_CMD_MF_READ;
  this->ndef_buffer_[1] = page;

  this->pcd_write_register(COMMAND_REG, PCD_IDLE);
  this->pcd_write_register(DIV_IRQ_REG, 0x04);
  this->pcd_write_register(FIFO_LEVEL_REG, 0x80);
  this->pcd_write_register(FIFO_DATA_REG, 2, this->ndef_buffer_);
  this->pcd_write_register(COMMAND_REG, PCD_CALC_CRC);

  bool crc_complete = false;
  uint32_t crc_start = millis();
  while (millis() - crc_start < RC522_MIFARE_OPERATION_TIMEOUT_MS) {
    uint8_t irq = this->pcd_read_register(DIV_IRQ_REG);
    if ((irq & 0x04) != 0) {
      crc_complete = true;
      break;
    }
    delayMicroseconds(100);
  }
  if (!crc_complete) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_TIMEOUT;
  }

  this->pcd_write_register(COMMAND_REG, PCD_IDLE);
  this->ndef_buffer_[2] = this->pcd_read_register(CRC_RESULT_REG_L);
  this->ndef_buffer_[3] = this->pcd_read_register(CRC_RESULT_REG_H);

  delayMicroseconds(RC522_MIFARE_TRANSCEIVE_DELAY_US);
  this->pcd_write_register(COMMAND_REG, PCD_IDLE);
  this->pcd_write_register(COM_IRQ_REG, 0x7F);
  this->pcd_write_register(FIFO_LEVEL_REG, 0x80);
  this->pcd_write_register(FIFO_DATA_REG, 4, this->ndef_buffer_);
  this->pcd_write_register(BIT_FRAMING_REG, 0x00);
  this->pcd_write_register(COMMAND_REG, PCD_TRANSCEIVE);
  this->pcd_set_register_bit_mask_(BIT_FRAMING_REG, 0x80);

  bool transceive_complete = false;
  uint32_t transceive_start = millis();
  while (millis() - transceive_start < RC522_MIFARE_OPERATION_TIMEOUT_MS) {
    uint8_t irq = this->pcd_read_register(COM_IRQ_REG);
    if ((irq & 0x01) != 0) {
      this->pcd_write_register(COMMAND_REG, PCD_IDLE);
      return STATUS_TIMEOUT;
    }
    if ((irq & RC522_WAIT_I_RQ) != 0) {
      transceive_complete = true;
      break;
    }
    delayMicroseconds(100);
  }
  if (!transceive_complete) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_TIMEOUT;
  }

  uint8_t error_reg_value = this->pcd_read_register(ERROR_REG);
  if ((error_reg_value & 0x13) != 0) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_ERROR;
  }
  if ((error_reg_value & 0x08) != 0) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_COLLISION;
  }

  const uint8_t valid_bits = this->pcd_read_register(CONTROL_REG) & 0x07;
  if (valid_bits != 0) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_ERROR;
  }

  const uint8_t fifo_length = this->pcd_read_register(FIFO_LEVEL_REG);
  if (fifo_length > sizeof(this->ndef_buffer_)) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return STATUS_NO_ROOM;
  }
  if (fifo_length < RC522_MIFARE_READ_SIZE) {
    this->pcd_write_register(COMMAND_REG, PCD_IDLE);
    return fifo_length == 1 ? STATUS_MIFARE_NACK : STATUS_ERROR;
  }

  this->pcd_read_register(FIFO_DATA_REG, fifo_length, this->ndef_buffer_, 0);
  this->pcd_write_register(COMMAND_REG, PCD_IDLE);

  for (uint8_t i = 0; i < RC522_MIFARE_READ_SIZE; i++) {
    data[i] = this->ndef_buffer_[i];
  }

  return STATUS_OK;
}

uint16_t RC522::read_mifare_ultralight_capacity_() { return this->nfc::Nfcc::read_mifare_ultralight_capacity_(); }

bool RC522::write_mifare_ultralight_page_(uint8_t page_num, const uint8_t *write_data, size_t len) {
  if (len != nfc::MIFARE_ULTRALIGHT_PAGE_SIZE) {
    return false;
  }

  uint8_t cmd[2 + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE + 2];
  cmd[0] = PICC_CMD_UL_WRITE;
  cmd[1] = page_num;
  memcpy(cmd + 2, write_data, len);
  this->pcd_calculate_crc_sync_(cmd, 2 + len, cmd + 2 + len);

  uint8_t response[2];
  uint8_t response_len = sizeof(response);
  uint8_t valid_bits = 0;
  StatusCode status = this->pcd_transceive_sync_raw_(cmd, sizeof(cmd), response, response_len, valid_bits);
  if (status != STATUS_OK) {
    ESP_LOGE(TAG, "Error writing page %u", page_num);
    return false;
  }

  if (!rc522_is_mifare_ack_(response, response_len, valid_bits)) {
    ESP_LOGE(TAG, "Page %u did not acknowledge write", page_num);
    return false;
  }

  return true;
}

bool RC522::nfc_write_mifare_ultralight_page_(uint16_t page, const std::vector<uint8_t> &data) {
  if (page > 0xFF) {
    return false;
  }
  return this->write_mifare_ultralight_page_(static_cast<uint8_t>(page), data.data(), data.size());
}

bool RC522::write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  return this->nfc::Nfcc::write_mifare_ultralight_tag_(uid, message);
}

bool RC522::clean_mifare_ultralight_() { return this->nfc::Nfcc::clean_mifare_ultralight_(); }

bool RC522::is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_data) {
  return this->nfc::Nfcc::is_mifare_ultralight_formatted_(page_data);
}

bool RC522::find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_data, uint8_t &message_length,
                                         uint8_t &message_start_index) {
  return this->nfc::Nfcc::find_mifare_ultralight_ndef_(page_data, message_length, message_start_index);
}

}  // namespace rc522
}  // namespace esphome
