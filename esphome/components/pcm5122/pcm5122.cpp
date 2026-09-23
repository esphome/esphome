#include "pcm5122.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::pcm5122 {

static const char *const TAG = "pcm5122";

void PCM5122::setup() {
  // Hold XSMT low (soft mute asserted) until init completes
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(false);
  }

  // Select page 0 and verify chip presence via I2C ACK
  if (!this->select_page_(0)) {
    ESP_LOGE(TAG, "Write failed");
    this->status_set_error(LOG_STR("Write failed"));
    this->mark_failed();
    return;
  }

  // Reset audio modules
  this->reg(PCM5122_REG_RESET) = PCM5122_RESET_MODULES;
  delay(20);
  this->reg(PCM5122_REG_RESET) = 0x00;

  // Ignore clock halt detection; enable clock divider autoset
  optional<uint8_t> err_detect = this->read_byte(PCM5122_REG_ERROR_DETECT);
  if (!err_detect.has_value()) {
    ESP_LOGE(TAG, "Failed to read ERROR_DETECT");
    this->mark_failed();
    return;
  }
  uint8_t err_detect_val = err_detect.value();
  err_detect_val |= PCM5122_ERROR_DETECT_IGNORE_CLKHALT;
  err_detect_val &= ~PCM5122_ERROR_DETECT_DISABLE_DIV_AUTOSET;
  this->reg(PCM5122_REG_ERROR_DETECT) = err_detect_val;

  // I2S format with the configured word length
  uint8_t alen;
  switch (this->bits_per_sample_) {
    case PCM5122_BITS_PER_SAMPLE_16:
      alen = PCM5122_AUDIO_FORMAT_ALEN_16BIT;
      break;
    case PCM5122_BITS_PER_SAMPLE_24:
      alen = PCM5122_AUDIO_FORMAT_ALEN_24BIT;
      break;
    case PCM5122_BITS_PER_SAMPLE_32:
    default:
      alen = PCM5122_AUDIO_FORMAT_ALEN_32BIT;
      break;
  }
  this->reg(PCM5122_REG_AUDIO_FORMAT) = PCM5122_AUDIO_FORMAT_I2S | alen;

  if (!this->write_channel_mix_()) {
    this->mark_failed();
    return;
  }

  if (!this->write_analog_gain_()) {
    this->mark_failed();
    return;
  }

  // PLL reference clock: BCK
  if (!this->select_page_(0)) {
    ESP_LOGE(TAG, "Write failed");
    this->mark_failed();
    return;
  }
  optional<uint8_t> pll_ref = this->read_byte(PCM5122_REG_PLL_REF);
  if (!pll_ref.has_value()) {
    ESP_LOGE(TAG, "Failed to read PLL_REF");
    this->mark_failed();
    return;
  }
  uint8_t pll_ref_val = pll_ref.value();
  pll_ref_val &= ~PCM5122_PLL_REF_MASK;
  pll_ref_val |= PCM5122_PLL_REF_SOURCE_BCK;
  this->reg(PCM5122_REG_PLL_REF) = pll_ref_val;

  if (!this->set_mute_on() || !this->set_volume(this->volume_)) {
    this->mark_failed();
    return;
  }

  // Release XSMT (soft un-mute) now that init has completed
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
  }
}

void PCM5122::dump_config() {
  const char *channel_mix_str;
  switch (this->channel_mix_) {
    case PCM5122_CHANNEL_MIX_LEFT_ONLY:
      channel_mix_str = "left only";
      break;
    case PCM5122_CHANNEL_MIX_RIGHT_ONLY:
      channel_mix_str = "right only";
      break;
    case PCM5122_CHANNEL_MIX_SWAPPED:
      channel_mix_str = "swapped";
      break;
    default:
      channel_mix_str = "stereo";
      break;
  }
  ESP_LOGCONFIG(TAG, "Audio DAC:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG,
                "  Bits per sample: %u\n"
                "  Analog gain: %s\n"
                "  Channel mix: %s\n"
                "  Volume range: %.1f dB to %.1f dB\n"
                "  Muted: %s",
                this->bits_per_sample_, this->analog_gain_ == PCM5122_ANALOG_GAIN_0DB ? "0 dB" : "-6 dB",
                channel_mix_str, this->volume_min_db_, this->volume_max_db_, YESNO(this->is_muted_));
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
}

bool PCM5122::set_mute_off() {
  this->is_muted_ = false;
  return this->write_mute_();
}

bool PCM5122::set_mute_on() {
  this->is_muted_ = true;
  return this->write_mute_();
}

bool PCM5122::set_volume(float volume) {
  this->volume_ = clamp<float>(volume, 0.0f, 1.0f);
  return this->write_volume_();
}

bool PCM5122::is_muted() { return this->is_muted_; }

float PCM5122::volume() { return this->volume_; }

bool PCM5122::select_page_(uint8_t page) {
  if (this->current_page_ == page)
    return true;
  if (!this->write_byte(PCM5122_REG_PAGE_SELECT, page)) {
    this->current_page_ = -1;
    return false;
  }
  this->current_page_ = page;
  return true;
}

bool PCM5122::write_mute_() {
  uint8_t mute_byte = this->is_muted() ? 0x11 : 0x00;
  if (!this->select_page_(0) || !this->write_byte(PCM5122_REG_MUTE, mute_byte)) {
    ESP_LOGE(TAG, "Writing mute failed");
    return false;
  }
  return true;
}

bool PCM5122::write_volume_() {
  // DVOL register: 0x00 = +24 dB, 0x30 = 0 dB, 0xFE = -103 dB, 0xFF = mute (-0.5 dB/step).
  // Note: volume=0.0 maps to volume_min_db_, which is not true silence unless set to -103 dB.
  // Use set_mute_on() for silence.
  const uint8_t dvol_max_volume = static_cast<uint8_t>(lroundf(0x30 - this->volume_max_db_ * 2.0f));
  const uint8_t dvol_min_volume = static_cast<uint8_t>(lroundf(0x30 - this->volume_min_db_ * 2.0f));

  const uint8_t volume_byte =
      dvol_max_volume + static_cast<uint8_t>(lroundf((1.0f - this->volume_) * (dvol_min_volume - dvol_max_volume)));

  ESP_LOGV(TAG, "Setting volume to 0x%.2x", volume_byte);

  if (!this->select_page_(0) || !this->write_byte(PCM5122_REG_DVOL_LEFT, volume_byte) ||
      !this->write_byte(PCM5122_REG_DVOL_RIGHT, volume_byte)) {
    ESP_LOGE(TAG, "Writing volume failed");
    return false;
  }
  return true;
}

bool PCM5122::write_analog_gain_() {
  uint8_t gain_byte = this->analog_gain_;
  if (!this->select_page_(1) || !this->write_byte(PCM5122_REG_ANALOG_GAIN, gain_byte)) {
    ESP_LOGE(TAG, "Writing analog gain failed");
    return false;
  }
  return true;
}

bool PCM5122::write_channel_mix_() {
  uint8_t channel_mix_byte = this->channel_mix_;
  if (!this->select_page_(0) || !this->write_byte(PCM5122_REG_DAC_DATA_PATH, channel_mix_byte)) {
    ESP_LOGE(TAG, "Writing channel mix failed");
    return false;
  }
  return true;
}

bool PCM5122::set_standby(bool enable) {
  bool prev_standby = this->standby_;
  this->standby_ = enable;
  if (!this->write_power_control_()) {
    this->standby_ = prev_standby;
    return false;
  }
  return true;
}

bool PCM5122::set_powerdown(bool enable) {
  bool prev_powerdown = this->powerdown_;
  this->powerdown_ = enable;
  if (!this->write_power_control_()) {
    this->powerdown_ = prev_powerdown;
    return false;
  }
  return true;
}

bool PCM5122::write_power_control_() {
  uint8_t power_byte =
      (this->standby_ ? PCM5122_POWER_CONTROL_RQST : 0) | (this->powerdown_ ? PCM5122_POWER_CONTROL_RQPD : 0);
  if (!this->select_page_(0) || !this->write_byte(PCM5122_REG_POWER_CONTROL, power_byte)) {
    ESP_LOGE(TAG, "Writing power control failed");
    return false;
  }
  return true;
}

}  // namespace esphome::pcm5122
