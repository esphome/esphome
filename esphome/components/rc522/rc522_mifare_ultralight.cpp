#include "rc522.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace rc522 {

static const char *const TAG = "rc522.mifare_ultralight";
static constexpr uint8_t RC522_MIFARE_READ_SIZE =
    nfc::MIFARE_ULTRALIGHT_PAGE_SIZE * nfc::MIFARE_ULTRALIGHT_READ_SIZE;
static constexpr uint8_t RC522_WAIT_I_RQ = 0x30;
static constexpr uint32_t RC522_MIFARE_OPERATION_TIMEOUT_MS = 500;
static constexpr uint16_t RC522_MIFARE_TRANSCEIVE_DELAY_US = 2000;

static bool rc522_is_mifare_ack_(const uint8_t *buffer, uint8_t buffer_length, uint8_t valid_bits) {
  return buffer_length == 1 && valid_bits == 4 && (buffer[0] & 0x0F) == nfc::MIFARE_CMD_ACK;
}

std::unique_ptr<nfc::NfcTag> RC522::read_mifare_ultralight_tag_(nfc::NfcTagUid &uid) {
  std::vector<uint8_t> data(RC522_MIFARE_READ_SIZE);
  if (this->read_mifare_ultralight_page_(3, data.data()) != STATUS_OK) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  if (!this->is_mifare_ultralight_formatted_(data)) {
    ESP_LOGW(TAG, "Not NDEF formatted");
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  uint8_t message_length;
  uint8_t message_start_index;
  if (!this->find_mifare_ultralight_ndef_(data, message_length, message_start_index)) {
    ESP_LOGW(TAG, "Couldn't find NDEF message");
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  ESP_LOGVV(TAG, "NDEF message length: %u, start: %u", message_length, message_start_index);

  if (message_length == 0) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }

  const uint16_t bytes_after_page_3 = message_length + message_start_index;
  uint16_t remaining_bytes = bytes_after_page_3 > 12 ? bytes_after_page_3 - 12 : 0;
  uint8_t current_page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE + 3;

  while (remaining_bytes > 0) {
    if (this->read_mifare_ultralight_page_(current_page, this->ndef_buffer_) != STATUS_OK) {
      ESP_LOGE(TAG, "Error reading tag data");
      return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
    }

    const uint8_t copy_length = std::min<uint16_t>(remaining_bytes, RC522_MIFARE_READ_SIZE);
    data.insert(data.end(), this->ndef_buffer_, this->ndef_buffer_ + copy_length);
    remaining_bytes -= copy_length;
    current_page += nfc::MIFARE_ULTRALIGHT_READ_SIZE;
  }

  data.erase(data.begin(), data.begin() + message_start_index + nfc::MIFARE_ULTRALIGHT_PAGE_SIZE);

  return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2, data);
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

uint16_t RC522::read_mifare_ultralight_capacity_() {
  if (this->read_mifare_ultralight_page_(3, this->ndef_buffer_) == STATUS_OK) {
    ESP_LOGV(TAG, "Tag capacity is %u bytes", this->ndef_buffer_[2] * 8U);
    return this->ndef_buffer_[2] * 8U;
  }
  return 0;
}

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
  uint32_t capacity = this->read_mifare_ultralight_capacity_();

  auto encoded = message->encode();

  uint32_t message_length = encoded.size();
  uint32_t buffer_length = nfc::get_mifare_ultralight_buffer_size(message_length);

  if (buffer_length > capacity) {
    ESP_LOGE(TAG, "Message length exceeds tag capacity %" PRIu32 " > %" PRIu32, buffer_length, capacity);
    return false;
  }

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
  uint8_t current_page = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  while (index < buffer_length) {
    if (!this->write_mifare_ultralight_page_(current_page, encoded.data() + index, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE)) {
      return false;
    }
    index += nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;
    current_page++;
  }
  return true;
}

bool RC522::clean_mifare_ultralight_() {
  uint32_t capacity = this->read_mifare_ultralight_capacity_();
  uint8_t pages = (capacity / nfc::MIFARE_ULTRALIGHT_PAGE_SIZE) + nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE;

  static constexpr std::array<uint8_t, nfc::MIFARE_ULTRALIGHT_PAGE_SIZE> BLANK_DATA = {0x00, 0x00, 0x00, 0x00};

  for (int i = nfc::MIFARE_ULTRALIGHT_DATA_START_PAGE; i < pages; i++) {
    if (!this->write_mifare_ultralight_page_(i, BLANK_DATA.data(), BLANK_DATA.size())) {
      return false;
    }
  }
  return true;
}

bool RC522::is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_data) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;

  return (page_data.size() > p4_offset + 3) &&
         ((page_data[p4_offset + 0] != 0xFF) || (page_data[p4_offset + 1] != 0xFF) ||
          (page_data[p4_offset + 2] != 0xFF) || (page_data[p4_offset + 3] != 0xFF));
}

bool RC522::find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_data, uint8_t &message_length,
                                         uint8_t &message_start_index) {
  const uint8_t p4_offset = nfc::MIFARE_ULTRALIGHT_PAGE_SIZE;

  if (!(page_data.size() > p4_offset + 6)) {
    return false;
  }

  if (page_data[p4_offset] == 0x03) {
    message_length = page_data[p4_offset + 1];
    message_start_index = 2;
    return true;
  }
  if (page_data[p4_offset + 5] == 0x03) {
    message_length = page_data[p4_offset + 6];
    message_start_index = 7;
    return true;
  }

  return false;
}

}  // namespace rc522
}  // namespace esphome
