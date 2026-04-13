#include "pcm5122.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pcm5122 {

static const char *const TAG = "pcm5122";

void PCM5122::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCM5122...");

  // Select page 0 and verify chip presence via I2C ACK
  if (!this->write_byte(PCM5122_REG_PAGE_SELECT, 0x00)) {
    ESP_LOGE(TAG, "PCM5122 not found");
    this->status_set_error(LOG_STR("PCM5122 not found"));
    this->mark_failed();
    return;
  }

  // Reset
  this->reg(PCM5122_REG_RESET) = 0x10;
  delay(20);
  this->reg(PCM5122_REG_RESET) = 0x00;

  // Ignore clock halt detection; enable clock divider autoset
  uint8_t err_detect = this->reg(PCM5122_REG_ERROR_DETECT).get();
  err_detect |= (1 << 3);   // ignore clock halt
  err_detect &= ~(1 << 1);  // enable clock divider autoset
  this->reg(PCM5122_REG_ERROR_DETECT) = err_detect;

  // 32-bit I2S
  this->reg(PCM5122_REG_AUDIO_FORMAT) = 3;

  // PLL reference clock: BCK (001)
  uint8_t pll_ref = this->reg(PCM5122_REG_PLL_REF).get();
  pll_ref &= ~(7 << 4);
  pll_ref |= (1 << 4);
  this->reg(PCM5122_REG_PLL_REF) = pll_ref;

  this->set_mute_on();
}

void PCM5122::dump_config() {
  ESP_LOGCONFIG(TAG, "PCM5122 Audio DAC:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Muted: %s", this->is_muted_ ? "Yes" : "No");
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

bool PCM5122::write_mute_() {
  uint8_t mute_byte = this->is_muted() ? 0x11 : 0x00;
  if (!this->write_byte(PCM5122_REG_PAGE_SELECT, 0x00) || !this->write_byte(PCM5122_REG_MUTE, mute_byte)) {
    ESP_LOGE(TAG, "Writing mute failed");
    return false;
  }
  return true;
}

bool PCM5122::write_volume_() {
  // DVOL register: 0x00 = +24 dB, 0x30 = 0 dB, 0xFF = mute (-0.5 dB/step)
  const uint8_t dvol_max_volume = 0x44;  // -10 dB at full scale
  const uint8_t dvol_min_volume = 0x99;  // -52.5 dB at minimum

  const uint8_t volume_byte =
      dvol_max_volume + static_cast<uint8_t>((1.0f - this->volume_) * (dvol_min_volume - dvol_max_volume) + 0.5f);

  ESP_LOGD(TAG, "Setting volume to 0x%.2x", volume_byte);

  if (!this->write_byte(PCM5122_REG_PAGE_SELECT, 0x00) || !this->write_byte(PCM5122_REG_DVOL_LEFT, volume_byte) ||
      !this->write_byte(PCM5122_REG_DVOL_RIGHT, volume_byte)) {
    ESP_LOGE(TAG, "Writing volume failed");
    return false;
  }
  return true;
}

}  // namespace pcm5122
}  // namespace esphome
