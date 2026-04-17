#include "pn532.h"

#include <algorithm>
#include <memory>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532 {

static const char *const TAG = "pn532";

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

struct Type4NdefMapping {
  uint16_t ndef_file_id;
  uint16_t max_ndef_file_size;
  uint16_t max_read_size;
  uint8_t write_access;
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

void PN532::setup() {
  // Get version data
  if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
    ESP_LOGW(TAG, "Error sending version command, trying again");
    if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
      ESP_LOGE(TAG, "Error sending version command");
      this->mark_failed();
      return;
    }
  }

  std::vector<uint8_t> version_data;
  if (!this->read_response(PN532_COMMAND_VERSION_DATA, version_data)) {
    ESP_LOGE(TAG, "Error getting version");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Found chip PN5%02X, Firmware v%d.%d", version_data[0], version_data[1], version_data[2]);

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
  if (!this->read_response(PN532_COMMAND_SAMCONFIGURATION, wakeup_result)) {
    this->error_code_ = WAKEUP_FAILED;
    this->mark_failed();
    return;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min<uint8_t>(255u, this->update_interval_ / 50);
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
  if (!this->read_response(PN532_COMMAND_SAMCONFIGURATION, sam_result)) {
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

bool PN532::powerdown() {
  updates_enabled_ = false;
  requested_read_ = false;
  ESP_LOGI(TAG, "Powering down PN532");
  if (!this->write_command_({PN532_COMMAND_POWERDOWN, 0b10100000})) {  // enable i2c,spi wakeup
    ESP_LOGE(TAG, "Error writing powerdown command to PN532");
    return false;
  }
  std::vector<uint8_t> response;
  if (!this->read_response(PN532_COMMAND_POWERDOWN, response)) {
    ESP_LOGE(TAG, "Error reading PN532 powerdown response");
    return false;
  }
  if (response[0] != 0x00) {
    ESP_LOGE(TAG, "Error on PN532 powerdown: %02x", response[0]);
    return false;
  }
  ESP_LOGV(TAG, "Powerdown successful");
  delay(1);
  return true;
}

void PN532::update() {
  if (!updates_enabled_)
    return;

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

  auto ready = this->read_ready_(false);
  if (ready == WOULDBLOCK)
    return;

  bool success = false;
  std::vector<uint8_t> read;

  if (ready == READY) {
    success = this->read_response(PN532_COMMAND_INLISTPASSIVETARGET, read);
  } else {
    this->send_ack_();  // abort still running InListPassiveTarget
  }

  this->requested_read_ = false;

  if (!success) {
    // Something failed
    if (!this->current_uid_.empty()) {
      auto tag = make_unique<nfc::NfcTag>(this->current_uid_);
      for (auto *trigger : this->triggers_ontagremoved_)
        trigger->process(tag);
    }
    this->current_uid_ = {};
    this->turn_off_rf_();
    return;
  }

  uint8_t num_targets = read[0];
  if (num_targets != 1) {
    // no tags found or too many
    if (!this->current_uid_.empty()) {
      auto tag = make_unique<nfc::NfcTag>(this->current_uid_);
      for (auto *trigger : this->triggers_ontagremoved_)
        trigger->process(tag);
    }
    this->current_uid_ = {};
    this->turn_off_rf_();
    return;
  }

  this->sak_ = read[4];
  uint8_t nfcid_length = read[5];
  if (nfcid_length > nfc::NFC_UID_MAX_LENGTH || read.size() < 6U + nfcid_length) {
    // oops, pn532 returned invalid data
    return;
  }
  nfc::NfcTagUid nfcid(read.begin() + 6, read.begin() + 6 + nfcid_length);

  bool report = true;
  for (auto *bin_sens : this->binary_sensors_) {
    if (bin_sens->process(nfcid)) {
      report = false;
    }
  }

  if (nfcid.size() == this->current_uid_.size()) {
    bool same_uid = true;
    for (size_t i = 0; i < nfcid.size(); i++)
      same_uid &= nfcid[i] == this->current_uid_[i];
    if (same_uid)
      return;
  }

  this->current_uid_ = nfcid;

  if (next_task_ == READ) {
    auto tag = this->read_tag_(nfcid);
    for (auto *trigger : this->triggers_ontag_)
      trigger->process(tag);

    if (report) {
      char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
      ESP_LOGD(TAG, "Found new tag '%s'", nfc::format_uid_to(uid_buf, nfcid));
      if (tag->has_ndef_message()) {
        const auto &message = tag->get_ndef_message();
        const auto &records = message->get_records();
        ESP_LOGD(TAG, "  NDEF formatted records:");
        for (const auto &record : records) {
          ESP_LOGD(TAG, "    %s - %s", record->get_type().c_str(), record->get_payload().c_str());
        }
      }
    }
  } else if (next_task_ == CLEAN) {
    ESP_LOGD(TAG, "  Tag cleaning");
    if (!this->clean_tag_(nfcid)) {
      ESP_LOGE(TAG, "  Tag was not fully cleaned successfully");
    }
    ESP_LOGD(TAG, "  Tag cleaned!");
  } else if (next_task_ == FORMAT) {
    ESP_LOGD(TAG, "  Tag formatting");
    if (!this->format_tag_(nfcid)) {
      ESP_LOGE(TAG, "Error formatting tag as NDEF");
    }
    ESP_LOGD(TAG, "  Tag formatted!");
  } else if (next_task_ == WRITE) {
    if (this->next_task_message_to_write_ != nullptr) {
      ESP_LOGD(TAG, "  Tag writing");
      ESP_LOGD(TAG, "  Tag formatting");
      if (!this->format_tag_(nfcid)) {
        ESP_LOGE(TAG, "  Tag could not be formatted for writing");
      } else {
        ESP_LOGD(TAG, "  Writing NDEF data");
        if (!this->write_tag_(nfcid, this->next_task_message_to_write_)) {
          ESP_LOGE(TAG, "  Failed to write message to tag");
        }
        ESP_LOGD(TAG, "  Finished writing NDEF data");
        delete this->next_task_message_to_write_;
        this->next_task_message_to_write_ = nullptr;
        this->on_finished_write_callback_.call();
      }
    }
  }

  this->read_mode();

  this->turn_off_rf_();
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

bool PN532::read_ack_() {
  ESP_LOGV(TAG, "Reading ACK");

  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return false;
  }

  bool matches = (data[1] == 0x00 &&                     // preamble
                  data[2] == 0x00 &&                     // start of packet
                  data[3] == 0xFF && data[4] == 0x00 &&  // ACK packet code
                  data[5] == 0xFF && data[6] == 0x00);   // postamble
  ESP_LOGV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

void PN532::send_ack_() {
  ESP_LOGV(TAG, "Sending ACK for abort");
  this->write_data({0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00});
  delay(10);
}
void PN532::send_nack_() {
  ESP_LOGV(TAG, "Sending NACK for retransmit");
  this->write_data({0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00});
  delay(10);
}

enum PN532ReadReady PN532::read_ready_(bool block) {
  if (this->rd_ready_ == READY) {
    if (block) {
      this->rd_start_time_.reset();
      this->rd_ready_ = WOULDBLOCK;
    }
    return READY;
  }

  if (!this->rd_start_time_.has_value()) {
    this->rd_start_time_ = millis();
  }

  while (true) {
    if (this->is_read_ready()) {
      this->rd_ready_ = READY;
      break;
    }

    if (millis() - *this->rd_start_time_ > 100) {
      ESP_LOGV(TAG, "Timed out waiting for readiness from PN532!");
      this->rd_ready_ = TIMEOUT;
      break;
    }

    if (!block) {
      this->rd_ready_ = WOULDBLOCK;
      break;
    }

    yield();
  }

  auto rdy = this->rd_ready_;
  if (block || rdy == TIMEOUT) {
    this->rd_start_time_.reset();
    this->rd_ready_ = WOULDBLOCK;
  }
  return rdy;
}

void PN532::turn_off_rf_() {
  ESP_LOGV(TAG, "Turning RF field OFF");
  this->write_command_({
      PN532_COMMAND_RFCONFIGURATION,
      0x01,  // RF Field
      0x00,  // Off
  });
}

std::unique_ptr<nfc::NfcTag> PN532::read_tag_(nfc::NfcTagUid &uid) {
  if ((this->sak_ & 0x20) != 0) {
    ESP_LOGD(TAG, "ISO-DEP / Type 4 tag");
    return this->read_iso_dep_tag_(uid);
  }

  uint8_t type = nfc::guess_tag_type(uid.size());

  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    ESP_LOGD(TAG, "Mifare classic");
    return this->read_mifare_classic_tag_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    ESP_LOGD(TAG, "Mifare ultralight");
    return this->read_mifare_ultralight_tag_(uid);
  } else if (type == nfc::TAG_TYPE_UNKNOWN) {
    ESP_LOGV(TAG, "Cannot determine tag type");
    return make_unique<nfc::NfcTag>(uid);
  } else {
    return make_unique<nfc::NfcTag>(uid);
  }
}

void PN532::read_mode() {
  this->next_task_ = READ;
  ESP_LOGD(TAG, "Waiting to read next tag");
}
void PN532::clean_mode() {
  this->next_task_ = CLEAN;
  ESP_LOGD(TAG, "Waiting to clean next tag");
}
void PN532::format_mode() {
  this->next_task_ = FORMAT;
  ESP_LOGD(TAG, "Waiting to format next tag");
}
void PN532::write_mode(nfc::NdefMessage *message) {
  this->next_task_ = WRITE;
  this->next_task_message_to_write_ = message;
  ESP_LOGD(TAG, "Waiting to write next tag");
}

bool PN532::clean_tag_(nfc::NfcTagUid &uid) {
  if ((this->sak_ & 0x20) != 0) {
    return this->clean_iso_dep_tag_();
  }

  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_mifare_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_();
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::format_tag_(nfc::NfcTagUid &uid) {
  if ((this->sak_ & 0x20) != 0) {
    return this->clean_iso_dep_tag_();
  }

  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_ndef_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_();
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::write_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  if ((this->sak_ & 0x20) != 0) {
    return this->write_iso_dep_tag_(message);
  }

  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->write_mifare_classic_tag_(uid, message);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->write_mifare_ultralight_tag_(uid, message);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
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

  LOG_UPDATE_INTERVAL(this);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

bool PN532BinarySensor::process(const nfc::NfcTagUid &data) {
  if (data.size() != this->uid_.size())
    return false;

  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}

bool PN532::iso_dep_send_apdu_(const std::vector<uint8_t> &apdu, std::vector<uint8_t> &response) {
  std::vector<uint8_t> cmd;
  cmd.reserve(apdu.size() + 2);
  cmd.push_back(PN532_COMMAND_INDATAEXCHANGE);
  cmd.push_back(0x01);
  cmd.insert(cmd.end(), apdu.begin(), apdu.end());

  if (!this->write_command_(cmd)) {
    return false;
  }

  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response) || response.empty() || response[0] != 0x00) {
    return false;
  }

  response.erase(response.begin());
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

bool PN532::nfc_iso_dep_transceive_(const std::vector<uint8_t> &send, std::vector<uint8_t> &response) {
  response.clear();
  std::vector<uint8_t> cmd;
  cmd.reserve(send.size() + 2);
  cmd.push_back(PN532_COMMAND_INDATAEXCHANGE);
  cmd.push_back(0x01);
  cmd.insert(cmd.end(), send.begin(), send.end());

  if (!this->write_command_(cmd)) {
    return false;
  }

  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, response) || response.empty() || response[0] != 0x00) {
    response.clear();
    return false;
  }

  response.erase(response.begin());
  return true;
}

std::unique_ptr<nfc::NfcTag> PN532::read_iso_dep_tag_(nfc::NfcTagUid &uid) {
  auto make_uid_only_tag = [&uid]() { return make_unique<nfc::NfcTag>(uid); };
  auto make_type_4_tag = [&uid]() { return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_4); };

  auto select_file = [this](uint16_t file_id) {
    std::vector<uint8_t> apdu = {ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02,
                                 static_cast<uint8_t>((file_id >> 8) & 0xFF),
                                 static_cast<uint8_t>(file_id & 0xFF)};
    std::vector<uint8_t> response;
    return this->iso_dep_send_apdu_(apdu, response);
  };

  auto read_binary = [this](uint16_t offset, uint8_t read_size, std::vector<uint8_t> &response) {
    std::vector<uint8_t> apdu = {ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((offset >> 8) & 0xFF),
                                 static_cast<uint8_t>(offset & 0xFF), read_size};
    return this->iso_dep_send_apdu_(apdu, response);
  };

  std::vector<uint8_t> response;
  std::vector<uint8_t> select_app_v2 = {ISO_CLA, ISO_INS_SELECT, 0x04, 0x00, 0x07,
                                        NDEF_APPLICATION_DFN_V2[0], NDEF_APPLICATION_DFN_V2[1],
                                        NDEF_APPLICATION_DFN_V2[2], NDEF_APPLICATION_DFN_V2[3],
                                        NDEF_APPLICATION_DFN_V2[4], NDEF_APPLICATION_DFN_V2[5],
                                        NDEF_APPLICATION_DFN_V2[6]};
  if (!this->iso_dep_send_apdu_(select_app_v2, response)) {
    std::vector<uint8_t> select_app_v1 = {ISO_CLA, ISO_INS_SELECT, 0x04, 0x00, 0x07,
                                          NDEF_APPLICATION_DFN_V1[0], NDEF_APPLICATION_DFN_V1[1],
                                          NDEF_APPLICATION_DFN_V1[2], NDEF_APPLICATION_DFN_V1[3],
                                          NDEF_APPLICATION_DFN_V1[4], NDEF_APPLICATION_DFN_V1[5],
                                          NDEF_APPLICATION_DFN_V1[6]};
    if (!this->iso_dep_send_apdu_(select_app_v1, response)) {
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

  return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_4, ndef_data);
}

bool PN532::clean_iso_dep_tag_() {
  std::vector<uint8_t> select_app_v2 = {ISO_CLA, ISO_INS_SELECT, 0x04, 0x00, 0x07,
                                        NDEF_APPLICATION_DFN_V2[0], NDEF_APPLICATION_DFN_V2[1],
                                        NDEF_APPLICATION_DFN_V2[2], NDEF_APPLICATION_DFN_V2[3],
                                        NDEF_APPLICATION_DFN_V2[4], NDEF_APPLICATION_DFN_V2[5],
                                        NDEF_APPLICATION_DFN_V2[6]};
  std::vector<uint8_t> response;
  if (!this->iso_dep_send_apdu_(select_app_v2, response)) {
    std::vector<uint8_t> select_app_v1 = {ISO_CLA, ISO_INS_SELECT, 0x04, 0x00, 0x07,
                                          NDEF_APPLICATION_DFN_V1[0], NDEF_APPLICATION_DFN_V1[1],
                                          NDEF_APPLICATION_DFN_V1[2], NDEF_APPLICATION_DFN_V1[3],
                                          NDEF_APPLICATION_DFN_V1[4], NDEF_APPLICATION_DFN_V1[5],
                                          NDEF_APPLICATION_DFN_V1[6]};
    if (!this->iso_dep_send_apdu_(select_app_v1, response)) {
      return false;
    }
  }

  std::vector<uint8_t> apdu = {ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02, CC_FILE_ID[0], CC_FILE_ID[1]};
  if (!this->iso_dep_send_apdu_(apdu, response)) {
    return false;
  }
  std::vector<uint8_t> cc_header;
  if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_READ_BINARY, 0x00, 0x00, 0x02}, cc_header) || cc_header.size() != 2) {
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
    if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((cc_offset >> 8) & 0xFF),
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

  if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02,
                                 static_cast<uint8_t>((mapping.ndef_file_id >> 8) & 0xFF),
                                 static_cast<uint8_t>(mapping.ndef_file_id & 0xFF)},
                                response)) {
    return false;
  }

  std::vector<uint8_t> clear_nlen = {ISO_CLA, ISO_INS_UPDATE_BINARY, 0x00, 0x00, 0x02, 0x00, 0x00};
  return this->iso_dep_send_apdu_(clear_nlen, response);
}

bool PN532::write_iso_dep_tag_(nfc::NdefMessage *message) {
  std::vector<uint8_t> select_app_v2 = {ISO_CLA, ISO_INS_SELECT, 0x04, 0x00, 0x07,
                                        NDEF_APPLICATION_DFN_V2[0], NDEF_APPLICATION_DFN_V2[1],
                                        NDEF_APPLICATION_DFN_V2[2], NDEF_APPLICATION_DFN_V2[3],
                                        NDEF_APPLICATION_DFN_V2[4], NDEF_APPLICATION_DFN_V2[5],
                                        NDEF_APPLICATION_DFN_V2[6]};
  std::vector<uint8_t> response;
  if (!this->iso_dep_send_apdu_(select_app_v2, response)) {
    std::vector<uint8_t> select_app_v1 = {ISO_CLA, ISO_INS_SELECT, 0x04, 0x00, 0x07,
                                          NDEF_APPLICATION_DFN_V1[0], NDEF_APPLICATION_DFN_V1[1],
                                          NDEF_APPLICATION_DFN_V1[2], NDEF_APPLICATION_DFN_V1[3],
                                          NDEF_APPLICATION_DFN_V1[4], NDEF_APPLICATION_DFN_V1[5],
                                          NDEF_APPLICATION_DFN_V1[6]};
    if (!this->iso_dep_send_apdu_(select_app_v1, response)) {
      return false;
    }
  }

  if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02, CC_FILE_ID[0], CC_FILE_ID[1]}, response)) {
    return false;
  }

  std::vector<uint8_t> cc_header;
  if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_READ_BINARY, 0x00, 0x00, 0x02}, cc_header) || cc_header.size() != 2) {
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
    if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_READ_BINARY, static_cast<uint8_t>((cc_offset >> 8) & 0xFF),
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

  if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_SELECT, 0x00, 0x0C, 0x02,
                                 static_cast<uint8_t>((mapping.ndef_file_id >> 8) & 0xFF),
                                 static_cast<uint8_t>(mapping.ndef_file_id & 0xFF)},
                                response)) {
    return false;
  }

  if (!this->iso_dep_send_apdu_({ISO_CLA, ISO_INS_UPDATE_BINARY, 0x00, 0x00, 0x02, 0x00, 0x00}, response)) {
    return false;
  }

  uint16_t offset = 0;
  while (offset < encoded.size()) {
    uint8_t chunk_size = std::min<uint16_t>(encoded.size() - offset, TYPE_4_WRITE_CHUNK_SIZE);
    std::vector<uint8_t> apdu = {ISO_CLA, ISO_INS_UPDATE_BINARY,
                                 static_cast<uint8_t>(((2 + offset) >> 8) & 0xFF),
                                 static_cast<uint8_t>((2 + offset) & 0xFF), chunk_size};
    apdu.insert(apdu.end(), encoded.begin() + offset, encoded.begin() + offset + chunk_size);
    if (!this->iso_dep_send_apdu_(apdu, response)) {
      return false;
    }
    offset += chunk_size;
  }

  uint16_t nlen = encoded.size();
  std::vector<uint8_t> nlen_apdu = {ISO_CLA, ISO_INS_UPDATE_BINARY, 0x00, 0x00, 0x02,
                                    static_cast<uint8_t>((nlen >> 8) & 0xFF), static_cast<uint8_t>(nlen & 0xFF)};
  return this->iso_dep_send_apdu_(nlen_apdu, response);
}

}  // namespace pn532
}  // namespace esphome
