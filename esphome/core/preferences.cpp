#include "preferences.h"
#include "esphome/core/log.h"

namespace esphome {

static const char *TAG = "preferences";

ESPPreferenceObject::ESPPreferenceObject() : offset_(0), length_words_(0), type_(0), data_(nullptr) {}
ESPPreferenceObject::ESPPreferenceObject(size_t offset, size_t length, uint32_t type)
    : offset_(offset), length_words_(length), type_(type) {
  this->data_ = new uint32_t[this->length_words_ + 1];
  for (uint32_t i = 0; i < this->length_words_ + 1; i++)
    this->data_[i] = 0;
}
bool ESPPreferenceObject::load_() {
  if (!this->is_initialized()) {
    ESP_LOGV(TAG, "Load Pref Not initialized!");
    return false;
  }
  if (!this->load_internal_())
    return false;

  bool valid = this->data_[this->length_words_] == this->calculate_crc_();

  ESP_LOGVV(TAG, "LOAD %u: valid=%s, 0=0x%08X 1=0x%08X (Type=%u, CRC=0x%08X)", this->offset_,  // NOLINT
            YESNO(valid), this->data_[0], this->data_[1], this->type_, this->calculate_crc_());
  return valid;
}
bool ESPPreferenceObject::save_() {
  if (!this->is_initialized()) {
    ESP_LOGV(TAG, "Save Pref Not initialized!");
    return false;
  }

  this->data_[this->length_words_] = this->calculate_crc_();
  if (!this->save_internal_())
    return false;
  ESP_LOGVV(TAG, "SAVE %u: 0=0x%08X 1=0x%08X (Type=%u, CRC=0x%08X)", this->offset_,  // NOLINT
            this->data_[0], this->data_[1], this->type_, this->calculate_crc_());
  return true;
}

ESPPreferences::ESPPreferences()
    // offset starts from start of user RTC mem (64 words before that are reserved for system),
    // an additional 32 words at the start of user RTC are for eboot (OTA, see eboot_command.h),
    // which will be reset each time OTA occurs
    : current_offset_(0) {}

uint32_t ESPPreferenceObject::calculate_crc_() const {
  uint32_t crc = this->type_;
  for (size_t i = 0; i < this->length_words_; i++) {
    crc ^= (this->data_[i] * 2654435769UL) >> 1;
  }
  return crc;
}
bool ESPPreferenceObject::is_initialized() const { return this->data_ != nullptr; }
}  // namespace esphome
