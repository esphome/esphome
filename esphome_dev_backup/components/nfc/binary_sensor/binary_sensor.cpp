#include "binary_sensor.h"
#include "../nfc_helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nfc {

static const char *const TAG = "nfc.binary_sensor";

void NfcTagBinarySensor::setup() {
  this->parent_->register_listener(this);
  this->publish_initial_state(false);
}

void NfcTagBinarySensor::dump_config() {
  std::string match_str = "name";

  LOG_BINARY_SENSOR("", "NFC Tag Binary Sensor", this);
  if (!this->match_string_.empty()) {
    if (!this->match_tag_name_) {
      match_str = "contains";
    }
    ESP_LOGCONFIG(TAG, "  Tag %s: %s", match_str.c_str(), this->match_string_.c_str());
    return;
  }
  if (!this->uid_.empty()) {
    ESP_LOGCONFIG(TAG, "  Tag UID: %s", format_bytes(this->uid_).c_str());
  }
}

void NfcTagBinarySensor::set_ndef_match_string(const std::string &str) {
  this->match_string_ = str;
  this->match_tag_name_ = false;
}

void NfcTagBinarySensor::set_tag_name(const std::string &str) {
  this->match_string_ = str;
  this->match_tag_name_ = true;
}

void NfcTagBinarySensor::set_uid(const std::vector<uint8_t> &uid) { this->uid_ = uid; }

bool NfcTagBinarySensor::tag_match_ndef_string(const std::shared_ptr<NdefMessage> &msg) {
  for (const auto &record : msg->get_records()) {
    if (record->get_payload().find(this->match_string_) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool NfcTagBinarySensor::tag_match_tag_name(const std::shared_ptr<NdefMessage> &msg) {
  for (const auto &record : msg->get_records()) {
    if (record->get_payload().find(HA_TAG_ID_PREFIX) != std::string::npos) {
      auto rec_substr = record->get_payload().substr(sizeof(HA_TAG_ID_PREFIX) - 1);
      if (rec_substr.find(this->match_string_) != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

bool NfcTagBinarySensor::tag_match_uid(const std::vector<uint8_t> &data) {
  if (data.size() != this->uid_.size()) {
    return false;
  }

  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i]) {
      return false;
    }
  }
  return true;
}

void NfcTagBinarySensor::tag_off(NfcTag &tag) {
  if (!this->match_string_.empty() && tag.has_ndef_message()) {
    if (this->match_tag_name_) {
      if (this->tag_match_tag_name(tag.get_ndef_message())) {
        this->publish_state(false);
      }
    } else {
      if (this->tag_match_ndef_string(tag.get_ndef_message())) {
        this->publish_state(false);
      }
    }
    return;
  }
  if (!this->uid_.empty() && this->tag_match_uid(tag.get_uid())) {
    this->publish_state(false);
  }
}

void NfcTagBinarySensor::tag_on(NfcTag &tag) {
  if (!this->match_string_.empty() && tag.has_ndef_message()) {
    if (this->match_tag_name_) {
      if (this->tag_match_tag_name(tag.get_ndef_message())) {
        this->publish_state(true);
      }
    } else {
      if (this->tag_match_ndef_string(tag.get_ndef_message())) {
        this->publish_state(true);
      }
    }
    return;
  }
  if (!this->uid_.empty() && this->tag_match_uid(tag.get_uid())) {
    this->publish_state(true);
  }
}

}  // namespace nfc
}  // namespace esphome
