#include "pn532.h"

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

void PN532::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PN532...");

  // Get version data
  if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
    ESP_LOGW(TAG, "Error sending version command, trying again...");
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
      ESP_LOGD(TAG, "Found new tag '%s'", nfc::format_uid(nfcid).c_str());
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
    ESP_LOGD(TAG, "  Tag cleaning...");
    if (!this->clean_tag_(nfcid)) {
      ESP_LOGE(TAG, "  Tag was not fully cleaned successfully");
    }
    ESP_LOGD(TAG, "  Tag cleaned!");
  } else if (next_task_ == FORMAT) {
    ESP_LOGD(TAG, "  Tag formatting...");
    if (!this->format_tag_(nfcid)) {
      ESP_LOGE(TAG, "Error formatting tag as NDEF");
    }
    ESP_LOGD(TAG, "  Tag formatted!");
  } else if (next_task_ == WRITE) {
    if (this->next_task_message_to_write_ != nullptr) {
      ESP_LOGD(TAG, "  Tag writing...");
      ESP_LOGD(TAG, "  Tag formatting...");
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
  ESP_LOGV(TAG, "Reading ACK...");

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
      this->rd_start_time_ = 0;
      this->rd_ready_ = WOULDBLOCK;
    }
    return READY;
  }

  if (!this->rd_start_time_) {
    this->rd_start_time_ = millis();
  }

  while (true) {
    if (this->is_read_ready()) {
      this->rd_ready_ = READY;
      break;
    }

    if (millis() - this->rd_start_time_ > 100) {
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
    this->rd_start_time_ = 0;
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

std::unique_ptr<nfc::NfcTag> PN532::read_tag_(std::vector<uint8_t> &uid) {
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

bool PN532::clean_tag_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_mifare_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_();
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::format_tag_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_ndef_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_();
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::write_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->write_mifare_classic_tag_(uid, message);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->write_mifare_ultralight_tag_(uid, message);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
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

  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}

}  // namespace pn532
}  // namespace esphome
