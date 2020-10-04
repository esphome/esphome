#include "pn532.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532 {

static const char *TAG = "pn532";

void PN532::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PN532...");
  this->spi_setup();

  // Wake the chip up from power down
  // 1. Enable the SS line for at least 2ms
  // 2. Send a dummy command to get the protocol synced up
  //    (this may time out, but that's ok)
  // 3. Send SAM config command with normal mode without waiting for ready bit (IRQ not initialized yet)
  // 4. Probably optional, send SAM config again, this time checking ACK and return value
  this->cs_->digital_write(false);
  delay(10);

  // send dummy firmware version command to get synced up
  this->pn532_write_command_check_ack_({0x02});  // get firmware version command
  // do not actually read any data, this should be OK according to datasheet

  this->pn532_write_command_({
      PN532_COMMAND_SAMCONFIGURATION,
      0x01,  // normal mode
      0x14,  // zero timeout (not in virtual card mode)
      0x01,
  });

  // do not wait for ready bit, this is a dummy command
  delay(2);

  // Try to read ACK, if it fails it might be because there's data from a previous power cycle left
  this->read_ack_();
  // do not wait for ready bit for return data
  delay(5);

  // read data packet for wakeup result
  auto wakeup_result = this->pn532_read_data_();
  if (wakeup_result.size() != 1) {
    this->error_code_ = WAKEUP_FAILED;
    this->mark_failed();
    return;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min(255u, this->update_interval_ / 50);
  bool ret = this->pn532_write_command_check_ack_({
      PN532_COMMAND_SAMCONFIGURATION,
      0x01,         // normal mode
      sam_timeout,  // timeout as multiple of 50ms (actually only for virtual card mode, but shouldn't matter)
      0x01,         // Enable IRQ
  });

  if (!ret) {
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  auto sam_result = this->pn532_read_data_();
  if (sam_result.size() != 1) {
    ESP_LOGV(TAG, "Invalid SAM result: (%u)", sam_result.size());  // NOLINT
    for (auto dat : sam_result) {
      ESP_LOGV(TAG, " 0x%02X", dat);
    }
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  this->turn_off_rf_();
}

void PN532::update() {
  for (auto *obj : this->binary_sensors_)
    obj->on_scan_end();

  bool success = this->pn532_write_command_check_ack_({
      PN532_COMMAND_INLISTPASSIVETARGET,
      0x01,  // max 1 card
      0x00,  // baud rate ISO14443A (106 kbit/s)
  });
  if (!success) {
    ESP_LOGW(TAG, "Requesting tag read failed!");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->requested_read_ = true;
}
void PN532::loop() {
  if (!this->requested_read_ || !this->is_ready_())
    return;

  std::vector<uint8_t> read = this->pn532_read_data_();
  this->requested_read_ = false;

  if (read.size() <= 2 || read[0] != 0x4B) {
    // Something failed
    this->turn_off_rf_();
    return;
  }

  uint8_t num_targets = read[1];
  if (num_targets != 1) {
    // no tags found or too many
    this->turn_off_rf_();
    return;
  }

  // const uint8_t target_number = read[2];
  // const uint16_t sens_res = uint16_t(read[3] << 8) | read[4];
  // const uint8_t sel_res = read[5];
  uint8_t nfcid_length = read[6];
  std::vector<uint8_t> nfcid(read[7], nfcid_length);
  if (read.size() < 7U + nfcid_length) {
    // oops, pn532 returned invalid data
    return;
  }

  nfc::NfcTag tag = this->read_tag_(nfcid);

  bool report = true;
  // 1. Go through all triggers
  for (auto *trigger : this->triggers_)
    trigger->process(&tag);

  // 2. Find a binary sensor
  for (auto *bin_sens : this->binary_sensors_) {
    if (bin_sens->process(nfcid)) {
      // 2.1 if found, do not dump
      report = false;
    }
  }

  if (report) {
    ESP_LOGD(TAG, "Found new tag '%s'", nfc::format_uid(nfcid).c_str());
  }

  this->turn_off_rf_();
}

nfc::NfcTag PN532::read_tag_(std::vector<uint8_t> uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());

  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    uint8_t key[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
    uint8_t current_block = 4;
    uint8_t message_start_index = 0;
    uint32_t message_length = 0;

    if (this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, key)) {
      std::vector<uint8_t> data = this->read_mifare_classic_block_(current_block);
      if (!data.empty()) {
        if (!nfc::decode_mifare_classic_tlv(data, message_length, message_start_index)) {
          return nfc::NfcTag(uid, "ERROR");
        }
      } else {
        ESP_LOGE(TAG, "Failed to read block %d", current_block);
        return nfc::NfcTag(uid, MIFARE_CLASSIC);
      }
    } else {
      ESP_LOGV(TAG, "Tag is not NDEF formatted");
      return nfc::NfcTag(uid, MIFARE_CLASSIC);
    }

    uint32_t index = 0;
    uint32_t buffer_size = nfc::get_buffer_size(message_length);
    std::vector<uint8_t> buffer;

    while (index < buffer_size) {
      if (nfc::mifare_classic_is_first_block(current_block)) {
        if (!this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, key)) {
          ESP_LOGE(TAG, "Error, Block authentication failed for %d", current_block);
        }
      }
      std::vector<uint8_t> data = this->read_mifare_classic_block_(current_block);
      if (!data.empty()) {
        buffer.insert(buffer.end(), data.begin(), data.end());
      } else {
        ESP_LOGE(TAG, "Error reading block %d", current_block);
      }

      index += nfc::BLOCK_SIZE;
      current_block++;

      if (nfc::mifare_classic_is_trailer_block(current_block)) {
        current_block++;
      }
    }

    return nfc::NfcTag(uid, MIFARE_CLASSIC, std::vector<uint8_t>(buffer.begin() + message_start_index, buffer.end()));
  } else if (type == nfc::TAG_TYPE_2) {
    // TODO: do the ultralight code
    return nfc::NfcTag(uid);
  } else if (type == nfc::TAG_TYPE_UNKNOWN) {
    ESP_LOGV(TAG, "Cannot determine tag type");
    return nfc::NfcTag(uid);
  } else {
    return nfc::NfcTag(uid);
  }
}

std::vector<uint8_t> PN532::read_mifare_classic_block_(uint8_t block_num) {
  this->pn532_write_command_({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_READ,
      block_num,
  });

  std::vector<uint8_t> response = this->pn532_read_data_();

  if (response.empty() || response[0] != 0x00) {
    return {};
  }
  return response;
}

bool PN532::auth_mifare_classic_block_(std::vector<uint8_t> uid, uint8_t block_num, uint8_t key_num, uint8_t *key) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,       // One card
      key_num,    // Mifare Key slot
      block_num,  // Block number
  });
  data.insert(data.end(), key, key + 6);
  data.insert(data.end(), uid.begin(), uid.end());
  this->pn532_write_command_(data);

  if (!this->wait_ready_())
    return false;

  std::vector<uint8_t> response = this->pn532_read_data_();

  if (response.empty() || response[0] != 0x00) {
    ESP_LOGE(TAG, "Authentication failed");
    return false;
  }

  return true;
}

void PN532::turn_off_rf_() {
  ESP_LOGVV(TAG, "Turning RF field OFF");
  this->pn532_write_command_check_ack_({
      PN532_COMMAND_RFCONFIGURATION,
      0x01,  // RF Field
      0x00,  // Off
  });
}

float PN532::get_setup_priority() const { return setup_priority::DATA; }

void PN532::pn532_write_command_(const std::vector<uint8_t> &data) {
  this->enable();
  delay(2);
  // First byte, communication mode: Write data
  this->write_byte(0x01);

  // Preamble
  this->write_byte(0x00);

  // Start code
  this->write_byte(0x00);
  this->write_byte(0xFF);

  // Length of message, TFI + data bytes
  const uint8_t real_length = data.size() + 1;
  // LEN
  this->write_byte(real_length);
  // LCS (Length checksum)
  this->write_byte(~real_length + 1);

  // TFI (Frame Identifier, 0xD4 means to PN532, 0xD5 means from PN532)
  this->write_byte(0xD4);
  // calculate checksum, TFI is part of checksum
  uint8_t checksum = 0xD4;

  // DATA
  for (uint8_t dat : data) {
    this->write_byte(dat);
    checksum += dat;
  }

  // DCS (Data checksum)
  this->write_byte(~checksum + 1);
  // Postamble
  this->write_byte(0x00);

  this->disable();
}

bool PN532::pn532_write_command_check_ack_(const std::vector<uint8_t> &data) {
  // 1. write command
  this->pn532_write_command_(data);

  // 2. wait for readiness
  if (!this->wait_ready_())
    return false;

  // 3. read ack
  if (!this->read_ack_()) {
    ESP_LOGV(TAG, "Invalid ACK frame received from PN532!");
    return false;
  }

  return true;
}

bool PN532::erase_tag_(nfc::NfcTag tag) {
  nfc::NdefMessage message = nfc::NdefMessage();
  message.add_empty_record();
  tag.set_ndef_message(&message);
  return this->write_tag_(tag);
}

bool PN532::format_tag_(nfc::NfcTag tag) {
  if (tag.get_tag_type() == MIFARE_CLASSIC) {
    return this->format_mifare_classic_ndef_(tag);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::clean_tag_(nfc::NfcTag tag) {
  if (tag.get_tag_type() == MIFARE_CLASSIC) {
    return this->format_mifare_classic_mifare_(tag);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::write_tag_(nfc::NfcTag tag) { return false; }

bool PN532::format_mifare_classic_mifare_(nfc::NfcTag tag) { return false; }

bool PN532::format_mifare_classic_ndef_(nfc::NfcTag tag) {
  uint8_t key_a[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  std::vector<uint8_t> empty_ndef_message(
      {0x03, 0x03, 0xD0, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  std::vector<uint8_t> sector_buffer_0(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  std::vector<uint8_t> sector_buffer_1(
      {0x14, 0x01, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1});
  std::vector<uint8_t> sector_buffer_2(
      {0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1});
  std::vector<uint8_t> sector_buffer_3(
      {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x78, 0x77, 0x88, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  std::vector<uint8_t> sector_buffer_4(
      {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07, 0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  if (!this->auth_mifare_classic_block_(tag.get_uid(), 0, nfc::MIFARE_CMD_AUTH_A, key_a)) {
    ESP_LOGE(TAG, "Unable to authenticate block 0 for formatting!");
    return false;
  }
  if (!this->write_mifare_classic_block_(1, sector_buffer_1))
    return false;
  if (!this->write_mifare_classic_block_(2, sector_buffer_2))
    return false;
  if (!this->write_mifare_classic_block_(3, sector_buffer_3))
    return false;

  for (int i = 4; i < 64; i += 4) {
    if (!this->auth_mifare_classic_block_(tag.get_uid(), i, nfc::MIFARE_CMD_AUTH_A, key_a)) {
      ESP_LOGE(TAG, "Failed to authenticate with block %d", i);
      continue;
    }
    if (i == 4) {
      if (!this->write_mifare_classic_block_(i, empty_ndef_message))
        ESP_LOGE(TAG, "Unable to write block %d", i);
    } else {
      if (!this->write_mifare_classic_block_(i, sector_buffer_0))
        ESP_LOGE(TAG, "Unable to write block %d", i);
    }
    if (!this->write_mifare_classic_block_(i + 1, sector_buffer_0))
      ESP_LOGE(TAG, "Unable to write block %d", i + 1);
    if (!this->write_mifare_classic_block_(i + 2, sector_buffer_0))
      ESP_LOGE(TAG, "Unable to write block %d", i + 2);
    if (!this->write_mifare_classic_block_(i + 3, sector_buffer_4))
      ESP_LOGE(TAG, "Unable to write block %d", i + 3);
  }
  return true;
}

bool PN532::write_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> write_data) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE,
      block_num,  // Block number
  });
  data.insert(data.end(), write_data.begin(), write_data.end());
  this->pn532_write_command_(data);

  if (!this->wait_ready_())
    return false;

  std::vector<uint8_t> response = this->pn532_read_data_();

  if (response.empty()) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  return true;
}

std::vector<uint8_t> PN532::pn532_read_data_() {
  this->enable();
  delay(2);
  // Read data (transmission from the PN532 to the host)
  this->write_byte(0x03);

  // sometimes preamble is not transmitted for whatever reason
  // mostly happens during startup.
  // just read the first two bytes and check if that is the case
  uint8_t header[6];
  this->read_array(header, 2);
  if (header[0] == 0x00 && header[1] == 0x00) {
    // normal packet, preamble included
    this->read_array(header + 2, 4);
  } else if (header[0] == 0x00 && header[1] == 0xFF) {
    // weird packet, preamble skipped; make it look like a normal packet
    header[0] = 0x00;
    header[1] = 0x00;
    header[2] = 0xFF;
    this->read_array(header + 3, 3);
  } else {
    // invalid packet
    this->disable();
    ESP_LOGV(TAG, "read data invalid preamble!");
    return {};
  }

  bool valid_header = (header[0] == 0x00 &&                                                      // preamble
                       header[1] == 0x00 &&                                                      // start code
                       header[2] == 0xFF && static_cast<uint8_t>(header[3] + header[4]) == 0 &&  // LCS, len + lcs = 0
                       header[5] == 0xD5  // TFI - frame from PN532 to system controller
  );
  if (!valid_header) {
    this->disable();
    ESP_LOGV(TAG, "read data invalid header!");
    return {};
  }

  std::vector<uint8_t> ret;
  // full length of message, including TFI
  const uint8_t full_len = header[3];
  // length of data, excluding TFI
  uint8_t len = full_len - 1;
  if (full_len == 0)
    len = 0;

  ret.resize(len);
  this->read_array(ret.data(), len);

  uint8_t checksum = 0xD5;
  for (uint8_t dat : ret)
    checksum += dat;
  checksum = ~checksum + 1;

  uint8_t dcs = this->read_byte();
  if (dcs != checksum) {
    this->disable();
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", dcs, checksum);
    return {};
  }

  if (this->read_byte() != 0x00) {
    this->disable();
    ESP_LOGV(TAG, "read data invalid postamble!");
    return {};
  }
  this->disable();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "PN532 Data Frame: (%u)", ret.size());  // NOLINT
  for (uint8_t dat : ret) {
    ESP_LOGVV(TAG, "  0x%02X", dat);
  }
#endif

  return ret;
}
bool PN532::is_ready_() {
  this->enable();
  // First byte, communication mode: Read state
  this->write_byte(0x02);
  // PN532 returns a single data byte,
  // "After having sent a command, the host controller must wait for bit 0 of Status byte equals 1
  // before reading the data from the PN532."
  bool ret = this->read_byte() == 0x01;
  this->disable();

  if (ret) {
    ESP_LOGVV(TAG, "Chip is ready!");
  }
  return ret;
}
bool PN532::read_ack_() {
  ESP_LOGVV(TAG, "Reading ACK...");
  this->enable();
  delay(2);
  // "Read data (transmission from the PN532 to the host) "
  this->write_byte(0x03);

  uint8_t ack[6];
  memset(ack, 0, sizeof(ack));

  this->read_array(ack, 6);
  this->disable();

  bool matches = (ack[0] == 0x00 &&                    // preamble
                  ack[1] == 0x00 &&                    // start of packet
                  ack[2] == 0xFF && ack[3] == 0x00 &&  // ACK packet code
                  ack[4] == 0xFF && ack[5] == 0x00     // postamble
  );
  ESP_LOGVV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}
bool PN532::wait_ready_() {
  uint32_t start_time = millis();
  while (!this->is_ready_()) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Timed out waiting for readiness from PN532!");
      return false;
    }
    yield();
  }
  return true;
}

void PN532::dump_config() {
  ESP_LOGCONFIG(TAG, "PN532:");
  switch (this->error_code_) {
    case NONE:
      break;
    case WAKEUP_FAILED:
      ESP_LOGE(TAG, "Wake Up command failed!");
      break;
    case SAM_COMMAND_FAILED:
      ESP_LOGE(TAG, "SAM command failed!");
      break;
  }

  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

bool PN532BinarySensor::process(std::vector<uint8_t> data) {
  if (data.size() != this->uid_.size())
    return false;

  for (uint8_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}
void PN532Trigger::process(nfc::NfcTag *tag) { this->trigger(nfc::format_uid(tag->get_uid()), *tag); }

}  // namespace pn532
}  // namespace esphome
