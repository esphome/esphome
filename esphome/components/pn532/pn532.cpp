#include "pn532.h"

#include <memory>
#include <algorithm>
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
  this->current_uids_.clear();
  this->persistent_tags_.clear();
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
  ESP_LOGD(TAG,
           "Found chip PN5%02X\n"
           "  Firmware ver. %d.%d",
           version_data[0], version_data[1], version_data[2]);

  if (version_data[1] != 0x01 || version_data[3] != 0x07) {
    ESP_LOGW(TAG,
             "PN532 firmware response looks non-standard (Ver: 0x%02X, Support: 0x%02X) - "
             "you may have a counterfeit chip which could cause unreliable behavior.",
             version_data[1], version_data[3]);
  }

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
  this->user_update_interval_ = this->update_interval_;
  this->sam_configured_ = true;

  // Set MaxRetries for passive activation to 2 (default is infinite 0xFF)
  // This makes the InListPassiveTarget command return much faster if no tag is present.
  this->write_command_({PN532_COMMAND_RFCONFIGURATION, 0x05, 0xFF, 0x01, 0x02});

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

  // If we are already waiting for a read or currently writing NDEF, skip this cycle.
  // This allows for a very fast update_interval (e.g. 500ms) without bus congestion.
  if (this->requested_read_ || (this->next_task_ == WRITE && this->next_task_message_to_write_ != nullptr)) {
    return;
  }

  if (!this->sam_configured_) {
    uint32_t now = millis();
    if (this->reinit_attempts_ > 0 && now - this->last_reinit_attempt_ < 100) {
      return;
    }
    this->last_reinit_attempt_ = now;

    if (this->reinit_()) {
      this->status_clear_warning();
      this->consecutive_failures_ = 0;
      this->reinit_attempts_ = 0;
      if (this->backoff_ms_ != 0) {
        this->backoff_ms_ = 0;
        this->set_update_interval(this->user_update_interval_);
      }
      this->requested_read_ = true;
    } else {
      this->status_set_warning();
      // Chip is unresponsive, increment missing counts
      for (auto &ptag : this->persistent_tags_) {
        ptag.missing_count++;
      }
      this->process_removed_tags_({});
      if (++this->reinit_attempts_ >= 5) {
        ESP_LOGW(TAG, "PN532 re-initialisation failed after 5 attempts");
        this->reinit_attempts_ = 0;
      }
    }
    return;
  }

  // Health check
  if (this->health_check_enabled_) {
    uint32_t now = millis();
    if (now - this->last_health_check_ >= this->health_check_interval_) {
      this->last_health_check_ = now;
      if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
        if (++this->consecutive_failures_ >= this->max_failed_checks_) {
          if (this->auto_reset_) {
            this->sam_configured_ = false;
          }
        }
        return;
      }
      std::vector<uint8_t> version_data;
      if (!this->read_response(PN532_COMMAND_VERSION_DATA, version_data)) {
        if (++this->consecutive_failures_ >= this->max_failed_checks_) {
          if (this->auto_reset_) {
            this->sam_configured_ = false;
          }
        }
        return;
      }
      this->consecutive_failures_ = 0;
    }
  }

  if (!this->write_command_({
          PN532_COMMAND_INLISTPASSIVETARGET,
          0x02,  // max 2 cards
          0x00,  // baud rate ISO14443A (106 kbit/s)
      })) {
    ESP_LOGW(TAG, "Requesting tag read failed!");
    this->status_set_warning();

    for (auto &ptag : this->persistent_tags_) {
      ptag.missing_count++;
    }
    this->process_removed_tags_({});

    uint32_t new_backoff =
        (this->backoff_ms_ == 0) ? this->user_update_interval_ * 2 : std::min(this->backoff_ms_ * 2, (uint32_t) 60000);
    if (new_backoff != this->backoff_ms_) {
      this->backoff_ms_ = new_backoff;
      this->set_update_interval(this->backoff_ms_);
    }
    if (++this->consecutive_failures_ >= this->max_failed_checks_) {
      if (this->auto_reset_) {
        this->sam_configured_ = false;
        this->reinit_attempts_ = 0;
      }
    }
    return;
  }

  this->status_clear_warning();
  this->consecutive_failures_ = 0;
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
    this->send_ack_();
    ESP_LOGV(TAG, "InListPassiveTarget failed/timeout (ready=%d)", ready);
  }

  this->requested_read_ = false;

  std::vector<std::vector<uint8_t>> new_uids;
  struct TargetInfo {
    uint8_t tg;
    std::vector<uint8_t> uid;
  };
  std::vector<TargetInfo> targets;

  if (success && !read.empty()) {
    uint8_t num_targets = read[0];
    uint8_t cursor = 1;
    for (uint8_t i = 0; i < num_targets; i++) {
      if (cursor + 5 > read.size()) {
        break;
      }
      uint8_t tg = read[cursor];
      uint8_t sel_res = read[cursor + 3];
      uint8_t nfcid_length = read[cursor + 4];
      if (cursor + 5 + nfcid_length > read.size()) {
        break;
      }
      std::vector<uint8_t> nfcid(read.begin() + cursor + 5, read.begin() + cursor + 5 + nfcid_length);
      new_uids.push_back(nfcid);
      targets.push_back({tg, nfcid});
      cursor += 5 + nfcid_length;
      if (sel_res & 0x20 && cursor < read.size()) {
        uint8_t ats_len = read[cursor];
        cursor += ats_len;
      }
    }
  }

  // Update missing counts
  for (auto &ptag : this->persistent_tags_) {
    bool found = false;
    for (const auto &new_uid : new_uids) {
      if (ptag.uid == new_uid) {
        found = true;
        break;
      }
    }
    if (!found) {
      ptag.missing_count++;
    } else {
      ptag.missing_count = 0;
    }
  }

  this->process_removed_tags_(new_uids);

  // Process added/present tags
  for (auto &target : targets) {
    bool is_known = false;
    for (auto &ptag : this->persistent_tags_) {
      if (target.uid == ptag.uid) {
        is_known = true;
        break;
      }
    }

    if (!is_known) {
      this->persistent_tags_.push_back({target.uid, 0});
      this->current_uids_.push_back(target.uid);
      auto tag = this->read_tag_(target.tg, target.uid);
      for (auto *trigger : this->triggers_ontag_)
        trigger->process(tag);

      NfcTagUid nfc_uid;
      nfc_uid.assign(target.uid.begin(), target.uid.end());
      ESP_LOGD(TAG, "Found new tag '%s'", nfc::format_uid(nfc_uid).c_str());

      if (next_task_ != READ) {
        if (next_task_ == CLEAN) {
          this->clean_tag_(target.tg, target.uid);
        } else if (next_task_ == FORMAT) {
          this->format_tag_(target.tg, target.uid);
        } else if (next_task_ == WRITE && this->next_task_message_to_write_ != nullptr) {
          if (this->format_tag_(target.tg, target.uid)) {
            if (this->write_tag_(target.tg, target.uid, this->next_task_message_to_write_)) {
              delete this->next_task_message_to_write_;
              this->next_task_message_to_write_ = nullptr;
              this->on_finished_write_callback_.call();
            }
          }
        }
        this->read_mode();
      }
    }
  }

  this->process_binary_sensors_();

  if (new_uids.empty() && !this->rf_field_enabled_) {
    this->turn_off_rf_();
  }
}

void PN532::process_removed_tags_(const std::vector<std::vector<uint8_t>> &new_uids) {
  for (auto it = this->persistent_tags_.begin(); it != this->persistent_tags_.end();) {
    if (it->missing_count >= 5) {
      std::vector<uint8_t> uid_copy = it->uid;

      NfcTagUid nfc_uid;
      nfc_uid.assign(uid_copy.begin(), uid_copy.end());
      auto tag = make_unique<nfc::NfcTag>(nfc_uid);

      ESP_LOGD(TAG, "Tag removed (threshold 5): %s", nfc::format_uid(nfc_uid).c_str());
      for (auto *trigger : this->triggers_ontagremoved_)
        trigger->process(tag);
      for (auto uit = this->current_uids_.begin(); uit != this->current_uids_.end(); ++uit) {
        if (*uit == uid_copy) {
          this->current_uids_.erase(uit);
          break;
        }
      }
      it = this->persistent_tags_.erase(it);
    } else {
      if (it->missing_count > 0) {
        NfcTagUid nfc_uid;
        nfc_uid.assign(it->uid.begin(), it->uid.end());
        ESP_LOGD(TAG, "Tag %s missing, count %d/5", nfc::format_uid(nfc_uid).c_str(), it->missing_count);
      }
      ++it;
    }
  }
}

void PN532::process_binary_sensors_() {
  for (auto *bin_sens : this->binary_sensors_) {
    bool found_in_persistent = false;
    for (const auto &ptag : this->persistent_tags_) {
      if (bin_sens->process(ptag.uid)) {  // bin_sens->process returns true and sets found_ = true if UID matches
        found_in_persistent = true;
        break;
      }
    }
    if (!found_in_persistent) {
      bin_sens->on_scan_end();  // This resets found_ and publishes false
    }
  }
}

bool PN532::write_command_(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> write_data;
  write_data.push_back(0x00);
  write_data.push_back(0x00);
  write_data.push_back(0xFF);
  const uint8_t real_length = data.size() + 1;
  write_data.push_back(real_length);
  write_data.push_back(~real_length + 1);
  write_data.push_back(0xD4);
  uint8_t checksum = 0xD4;
  for (uint8_t dat : data) {
    write_data.push_back(dat);
    checksum += dat;
  }
  write_data.push_back(~checksum + 1);
  write_data.push_back(0x00);
  this->write_data(write_data);
  return this->read_ack_();
}

bool PN532::read_ack_() {
  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return false;
  }
  return (data[1] == 0x00 && data[2] == 0x00 && data[3] == 0xFF && data[4] == 0x00 && data[5] == 0xFF &&
          data[6] == 0x00);
}

void PN532::send_ack_() { this->write_data({0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00}); }
void PN532::send_nack_() { this->write_data({0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00}); }

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
      this->rd_latency_ms_ = millis() - this->rd_start_time_;
      this->rd_ready_ = READY;
      break;
    }
    if (millis() - this->rd_start_time_ > 200) {
      this->rd_latency_ms_ = 200;
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

void PN532::turn_off_rf_() { this->write_command_({PN532_COMMAND_RFCONFIGURATION, 0x01, 0x00}); }

std::unique_ptr<nfc::NfcTag> PN532::read_tag_(uint8_t tg, std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->read_mifare_classic_tag_(tg, uid);
  }
  if (type == nfc::TAG_TYPE_2) {
    return this->read_mifare_ultralight_tag_(tg, uid);
  }
  NfcTagUid nfc_uid;
  nfc_uid.assign(uid.begin(), uid.end());
  return make_unique<nfc::NfcTag>(nfc_uid);
}

void PN532::read_mode() { this->next_task_ = READ; }
void PN532::clean_mode() { this->next_task_ = CLEAN; }
void PN532::format_mode() { this->next_task_ = FORMAT; }
void PN532::write_mode(nfc::NdefMessage *message) {
  this->next_task_ = WRITE;
  this->next_task_message_to_write_ = message;
}

bool PN532::clean_tag_(uint8_t tg, std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_mifare_(tg, uid);
  }
  if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_(tg);
  }
  return false;
}

bool PN532::format_tag_(uint8_t tg, std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_ndef_(tg, uid);
  }
  if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_(tg);
  }
  return false;
}

bool PN532::write_tag_(uint8_t tg, std::vector<uint8_t> &uid, nfc::NdefMessage *message) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->write_mifare_classic_tag_(tg, uid, message);
  }
  if (type == nfc::TAG_TYPE_2) {
    return this->write_mifare_ultralight_tag_(tg, uid, message);
  }
  return false;
}

bool PN532::reinit_() {
  ESP_LOGW(TAG, "Attempting PN532 re-initialisation...");
  if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
    return false;
  }
  std::vector<uint8_t> ver;
  if (!this->read_response(PN532_COMMAND_VERSION_DATA, ver)) {
    return false;
  }
  if (!this->write_command_({PN532_COMMAND_SAMCONFIGURATION, 0x01, 0x14, 0x01})) {
    return false;
  }
  std::vector<uint8_t> wakeup_result;
  if (!this->read_response(PN532_COMMAND_SAMCONFIGURATION, wakeup_result)) {
    return false;
  }
  uint8_t sam_timeout = std::min<uint8_t>(255u, this->update_interval_ / 50);
  if (!this->write_command_({PN532_COMMAND_SAMCONFIGURATION, 0x01, sam_timeout, 0x01})) {
    return false;
  }
  std::vector<uint8_t> sam_result;
  if (!this->read_response(PN532_COMMAND_SAMCONFIGURATION, sam_result)) {
    return false;
  }
  if (!this->rf_field_enabled_) {
    this->turn_off_rf_();
  }
  this->sam_configured_ = true;

  // Set MaxRetries for passive activation to 2
  this->write_command_({PN532_COMMAND_RFCONFIGURATION, 0x05, 0xFF, 0x01, 0x02});

  ESP_LOGI(TAG, "PN532 re-initialised successfully!");
  return true;
}

void PN532::dump_config() {
  ESP_LOGCONFIG(TAG, "PN532:");
  LOG_UPDATE_INTERVAL(this);
  for (auto *child : this->binary_sensors_)
    LOG_BINARY_SENSOR("  ", "Tag", child);
}

bool PN532BinarySensor::process(const std::vector<uint8_t> &data) {
  if (data.size() != this->uid_.size()) {
    return false;
  }
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i]) {
      return false;
    }
  }
  this->publish_state(true);
  this->found_ = true;
  return true;
}

}  // namespace pn532
}  // namespace esphome
