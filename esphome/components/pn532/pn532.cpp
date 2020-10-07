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

  // Get version data
  if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
    ESP_LOGE(TAG, "Error sending version command");
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> version_data;
  if (!this->read_response_(PN532_COMMAND_VERSION_DATA, version_data)) {
    ESP_LOGE(TAG, "Error getting version");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Found chip PN5%02X", version_data[0]);
  ESP_LOGD(TAG, "Firmware ver. %d.%d", version_data[1], version_data[2]);

  if (!this->write_command_({
          PN532_COMMAND_SAMCONFIGURATION,
          0x01,  // normal mode
          0x14,  // zero timeout (not in virtual card mode)
          0x01,
      })) {
    ESP_LOGE(TAG, "No wakeup ack");
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> wakeup_result;
  if (!this->read_response_(PN532_COMMAND_SAMCONFIGURATION, wakeup_result)) {
    this->error_code_ = WAKEUP_FAILED;
    this->mark_failed();
    return;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min(255u, this->update_interval_ / 50);
  if (!this->write_command_({
          PN532_COMMAND_SAMCONFIGURATION,
          0x01,         // normal mode
          sam_timeout,  // timeout as multiple of 50ms (actually only for virtual card mode, but shouldn't matter)
          0x01,         // Enable IRQ
      })) {
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> sam_result;
  if (!this->read_response_(PN532_COMMAND_SAMCONFIGURATION, sam_result)) {
    ESP_LOGV(TAG, "Invalid SAM result: (%u)", sam_result.size());  // NOLINT
    for (uint8_t dat : sam_result) {
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

  if (!this->write_command_({
          PN532_COMMAND_INLISTPASSIVETARGET,
          0x01,  // max 1 card
          0x00,  // baud rate ISO14443A (106 kbit/s)
      })) {
    ESP_LOGW(TAG, "Requesting tag read failed!");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->requested_read_ = true;
}

void PN532::loop() {
  if (!this->requested_read_)
    return;

  std::vector<uint8_t> read;
  bool success = this->read_response_(PN532_COMMAND_INLISTPASSIVETARGET, read);

  this->requested_read_ = false;

  if (!success) {
    // Something failed
    this->current_uid_ = {};
    this->turn_off_rf_();
    return;
  }

  uint8_t num_targets = read[0];
  if (num_targets != 1) {
    // no tags found or too many
    this->current_uid_ = {};
    this->turn_off_rf_();
    return;
  }

  uint8_t nfcid_length = read[5];
  std::vector<uint8_t> nfcid(read.begin() + 6, read.begin() + 6 + nfcid_length);
  if (read.size() < 6U + nfcid_length) {
    // oops, pn532 returned invalid data
    return;
  }

  bool report = true;
  for (auto *bin_sens : this->binary_sensors_) {
    if (bin_sens->process(nfcid)) {
      report = false;
    }
  }

  if (nfcid.size() == this->current_uid_.size()) {
    bool same_uid = false;
    for (uint8_t i = 0; i < nfcid.size(); i++)
      same_uid |= nfcid[i] == this->current_uid_[i];
    if (same_uid)
      return;
  }

  this->current_uid_ = nfcid;

  auto tag = this->read_tag_(nfcid);

  if (next_task_ == READ) {
    for (auto *trigger : this->triggers_)
      trigger->process(tag);

    if (report) {
      ESP_LOGD(TAG, "Found tag '%s'", nfc::format_uid(nfcid).c_str());
      if (tag->has_ndef_message()) {
        auto message = tag->get_ndef_message();
        auto records = message->get_records();
        ESP_LOGD(TAG, "  NDEF formatted records:");
        for (auto &record : records) {
          ESP_LOGD(TAG, "    %s - %s", record->get_type().c_str(), record->get_payload().c_str());
        }
      }
    }
  } else if (next_task_ == CLEAN) {
    if (!this->clean_tag_(nfcid)) {
      ESP_LOGE(TAG, "Tag was not fully cleaned successfully");
    }
  } else if (next_task_ == FORMAT) {
    if (!this->format_tag_(nfcid)) {
      ESP_LOGE(TAG, "Error formatting tag as NDEF");
    }
  } else if (next_task_ == ERASE) {
    if (!this->erase_tag_(nfcid)) {
      ESP_LOGE(TAG, "Tag was not erased successfully");
    }
  } else if (next_task_ == WRITE) {
    if (this->next_task_message_to_write_ != nullptr) {
      if (tag->has_ndef_message()) {
        // if (!this->erase_tag_(nfcid)) {
        //   ESP_LOGE(TAG, "Tag could not be erased for writing");
        //   this->turn_off_rf_();
        //   return;
        // }
      } else {
        if (!this->format_tag_(nfcid)) {
          ESP_LOGE(TAG, "Tag could not be formatted for writing");
          this->turn_off_rf_();
          return;
        }
      }
      if (!this->write_tag_(nfcid, this->next_task_message_to_write_)) {
        ESP_LOGE(TAG, "Failed to write message to tag");
      }
    }
  }

  if (!this->next_task_continuous_) {
    this->next_task_ = READ;
  }

  this->turn_off_rf_();
}

nfc::NfcTag *PN532::read_tag_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());

  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    uint8_t current_block = 4;
    uint8_t message_start_index = 0;
    uint32_t message_length = 0;

    if (this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY)) {
      std::vector<uint8_t> data = this->read_mifare_classic_block_(current_block);
      if (!data.empty()) {
        if (!nfc::decode_mifare_classic_tlv(data, message_length, message_start_index)) {
          return new nfc::NfcTag(uid, nfc::ERROR);
        }
      } else {
        ESP_LOGE(TAG, "Failed to read block %d", current_block);
        return new nfc::NfcTag(uid, nfc::MIFARE_CLASSIC);
      }
    } else {
      ESP_LOGV(TAG, "Tag is not NDEF formatted");
      return new nfc::NfcTag(uid, nfc::MIFARE_CLASSIC);
    }

    uint32_t index = 0;
    uint32_t buffer_size = nfc::get_buffer_size(message_length);
    std::vector<uint8_t> buffer;

    while (index < buffer_size) {
      if (nfc::mifare_classic_is_first_block(current_block)) {
        if (!this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY)) {
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
    buffer.erase(buffer.begin(), buffer.begin() + message_start_index);
    return new nfc::NfcTag(uid, nfc::MIFARE_CLASSIC, buffer);
  } else if (type == nfc::TAG_TYPE_2) {
    // TODO: do the ultralight code
    return new nfc::NfcTag(uid);
  } else if (type == nfc::TAG_TYPE_UNKNOWN) {
    ESP_LOGV(TAG, "Cannot determine tag type");
    return new nfc::NfcTag(uid);
  } else {
    return new nfc::NfcTag(uid);
  }
}

std::vector<uint8_t> PN532::read_mifare_classic_block_(uint8_t block_num) {
  if (!this->write_command_({
          PN532_COMMAND_INDATAEXCHANGE,
          0x01,  // One card
          nfc::MIFARE_CMD_READ,
          block_num,
      })) {
    return {};
  }

  std::vector<uint8_t> response;
  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, response) || response[0] != 0x00) {
    return {};
  }
  response.erase(response.begin());
  return response;
}

bool PN532::auth_mifare_classic_block_(std::vector<uint8_t> &uid, uint8_t block_num, uint8_t key_num,
                                       const uint8_t *key) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,       // One card
      key_num,    // Mifare Key slot
      block_num,  // Block number
  });
  data.insert(data.end(), key, key + 6);
  data.insert(data.end(), uid.begin(), uid.end());
  if (!this->write_command_(data)) {
    ESP_LOGE(TAG, "Authentication failed");
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, response) || response[0] != 0x00) {
    ESP_LOGE(TAG, "Authentication failed");
    return false;
  }

  return true;
}

void PN532::turn_off_rf_() {
  ESP_LOGVV(TAG, "Turning RF field OFF");
  this->write_command_({
      PN532_COMMAND_RFCONFIGURATION,
      0x01,  // RF Field
      0x00,  // Off
  });
}

bool PN532::write_command_(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> write_data;
  // Preamble
  write_data.push_back(0x00);

  // Start code
  write_data.push_back(0x00);
  write_data.push_back(0xFF);

  // Length of message, TFI + data bytes
  const uint8_t real_length = data.size() + 1;
  // LEN
  write_data.push_back(real_length);
  // LCS (Length checksum)
  write_data.push_back(~real_length + 1);

  // TFI (Frame Identifier, 0xD4 means to PN532, 0xD5 means from PN532)
  write_data.push_back(0xD4);
  // calculate checksum, TFI is part of checksum
  uint8_t checksum = 0xD4;

  // DATA
  for (uint8_t dat : data) {
    write_data.push_back(dat);
    checksum += dat;
  }

  // DCS (Data checksum)
  write_data.push_back(~checksum + 1);
  // Postamble
  write_data.push_back(0x00);

  this->write_data(write_data);

  return this->read_ack_();
}

bool PN532::read_response_(uint8_t command, std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Reading response");
  uint8_t len = this->read_response_length_();
  if (len == 0) {
    return false;
  }

  ESP_LOGV(TAG, "Reading response of length %d", len);
  if (!this->read_data(data, 6 + len + 2)) {
    ESP_LOGD(TAG, "No response data");
    return false;
  }

  if (data[1] != 0x00 && data[2] != 0x00 && data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return false;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5 &&                               // TFI - frame from PN532 to system controller
                       data[7] == command + 1);                         // Correct command response

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return false;
  }

  data.erase(data.begin(), data.begin() + 6);  // Remove headers

  uint8_t checksum = 0;
  for (int i = 0; i < len + 1; i++) {
    uint8_t dat = data[i];
    checksum += dat;
  }
  checksum = ~checksum + 1;

  if (data[len + 1] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", data[len], checksum);
    return false;
  }

  if (data[len + 2] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return false;
  }

  data.erase(data.begin(), data.begin() + 2);  // Remove TFI and command code
  data.erase(data.end() - 2, data.end());      // Remove checksum and postamble

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGD(TAG, "PN532 Data Frame: (%u)", data.size());  // NOLINT
  for (uint8_t dat : data) {
    ESP_LOGD(TAG, "  0x%02X", dat);
  }
#endif

  return true;
}

void PN532::clean_tag(bool continuous) {
  this->next_task_ = CLEAN;
  this->next_task_continuous_ = continuous;
  ESP_LOGD(TAG, "Waiting to clean next tag");
}
void PN532::erase_tag(bool continuous) {
  this->next_task_ = ERASE;
  this->next_task_continuous_ = continuous;
  ESP_LOGD(TAG, "Waiting to erase next tag");
}
void PN532::format_tag(bool continuous) {
  this->next_task_ = FORMAT;
  this->next_task_continuous_ = continuous;
  ESP_LOGD(TAG, "Waiting to format next tag");
}
void PN532::write_tag(nfc::NdefMessage *message, bool continuous) {
  this->next_task_ = WRITE;
  this->next_task_continuous_ = continuous;
  this->next_task_message_to_write_ = message;
  ESP_LOGD(TAG, "Waiting to write next tag");
}

bool PN532::erase_tag_(std::vector<uint8_t> &uid) {
  auto message = new nfc::NdefMessage();
  message->add_empty_record();
  return this->write_tag_(uid, message);
}

bool PN532::format_tag_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_ndef_(uid);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::clean_tag_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_mifare_(uid);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::write_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message) {
  auto encoded = message->encode();

  if (encoded.size() < 255) {
    encoded.insert(encoded.begin(), 0x03);
    encoded.insert(encoded.begin() + 1, encoded.size());
    encoded.push_back(0xFE);
  } else {
    encoded.insert(encoded.begin(), 0x03);
    encoded.insert(encoded.begin() + 1, 0xFF);
    encoded.insert(encoded.begin() + 2, (encoded.size() >> 8) & 0xFF);
    encoded.insert(encoded.begin() + 2, encoded.size() & 0xFF);
    encoded.push_back(0xFE);
  }

  uint32_t index = 0;
  uint8_t current_block = 4;

  while (index < encoded.size()) {
    if (nfc::mifare_classic_is_first_block(current_block)) {
      ESP_LOGD(TAG, "Trying to auth %d", current_block);
      if (!this->auth_mifare_classic_block_(uid, current_block, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY)) {
        return false;
      }
    }

    std::vector<uint8_t> data(encoded.begin() + index, encoded.begin() + index + nfc::BLOCK_SIZE);
    if (!this->write_mifare_classic_block_(current_block, data)) {
      return false;
    }
    index += nfc::BLOCK_SIZE;
    current_block++;

    if (nfc::mifare_classic_is_trailer_block(current_block)) {
      // Skipping as cannot write to trailer
      current_block++;
    }
  }
  return true;
}

bool PN532::format_mifare_classic_mifare_(std::vector<uint8_t> &uid) {
  std::vector<uint8_t> blank_buffer(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  std::vector<uint8_t> trailer_buffer(
      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x80, 0x69, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  bool error = false;

  for (int idx = 0; idx < 16; idx++) {
    if (!this->auth_mifare_classic_block_(uid, (4 * idx) + 3, nfc::MIFARE_CMD_AUTH_B, nfc::DEFAULT_KEY)) {
      ESP_LOGE(TAG, "No keys work!!! sector %d", idx);
      continue;
    }

    if (idx == 0) {
      if (!this->write_mifare_classic_block_((4 * idx) + 1, blank_buffer)) {
        ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx) + 1);
        error = true;
      }
    } else {
      if (!this->write_mifare_classic_block_((4 * idx), blank_buffer)) {
        ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx));
        error = true;
      }
      if (!this->write_mifare_classic_block_((4 * idx) + 1, blank_buffer)) {
        ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx) + 1);
        error = true;
      }
    }

    if (!this->write_mifare_classic_block_((4 * idx) + 2, blank_buffer)) {
      ESP_LOGE(TAG, "Unable to write sector %d-%d", idx, (4 * idx) + 2);
      error = true;
    }

    if (!this->write_mifare_classic_block_((4 * idx) + 3, trailer_buffer)) {
      ESP_LOGE(TAG, "Unable to write trailer of sector %d-%d", idx, (4 * idx) + 3);
      error = true;
    }
  }

  return !error;
}

bool PN532::format_mifare_classic_ndef_(std::vector<uint8_t> &uid) {
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

  if (!this->auth_mifare_classic_block_(uid, 0, nfc::MIFARE_CMD_AUTH_A, nfc::DEFAULT_KEY)) {
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
    if (!this->auth_mifare_classic_block_(uid, i, nfc::MIFARE_CMD_AUTH_A, nfc::DEFAULT_KEY)) {
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

bool PN532::write_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &write_data) {
  std::vector<uint8_t> data({
      PN532_COMMAND_INDATAEXCHANGE,
      0x01,  // One card
      nfc::MIFARE_CMD_WRITE,
      block_num,  // Block number
  });
  data.insert(data.end(), write_data.begin(), write_data.end());
  if (!this->write_command_(data)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  std::vector<uint8_t> response;
  if (!this->read_response_(PN532_COMMAND_INDATAEXCHANGE, response)) {
    ESP_LOGE(TAG, "Error writing block %d", block_num);
    return false;
  }

  return true;
}

uint8_t PN532::read_response_length_() {
  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return 0;
  }

  if (data[1] != 0x00 && data[2] != 0x00 && data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return 0;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5);                                // TFI - frame from PN532 to system controller

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return 0;
  }

  this->write_data({0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00});  // NACK - Retransmit last message

  // full length of message, including TFI
  uint8_t full_len = data[4];
  // length of data, excluding TFI
  uint8_t len = full_len - 1;
  if (full_len == 0)
    len = 0;
  return len;
}

bool PN532::read_ack_() {
  ESP_LOGVV(TAG, "Reading ACK...");

  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return false;
  }

  bool matches = (data[1] == 0x00 &&                     // preamble
                  data[2] == 0x00 &&                     // start of packet
                  data[3] == 0xFF && data[4] == 0x00 &&  // ACK packet code
                  data[5] == 0xFF && data[6] == 0x00);   // postamble
  ESP_LOGVV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

float PN532::get_setup_priority() const { return setup_priority::DATA; }

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

  LOG_UPDATE_INTERVAL(this);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

bool PN532BinarySensor::process(std::vector<uint8_t> &data) {
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
