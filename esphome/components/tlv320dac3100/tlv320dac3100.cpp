#include "tlv320dac3100.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tlv320dac3100 {

ESPHOME_LOG_TAG(TAG, "tlv320dac3100");

// Page 0
static constexpr uint8_t REG_PAGE = 0x00;
static constexpr uint8_t REG_RESET = 0x01;
static constexpr uint8_t REG_CLOCK_MUX = 0x04;
static constexpr uint8_t REG_PLL_P_R = 0x05;
static constexpr uint8_t REG_PLL_J = 0x06;
static constexpr uint8_t REG_PLL_D_MSB = 0x07;
static constexpr uint8_t REG_PLL_D_LSB = 0x08;
static constexpr uint8_t REG_NDAC = 0x0B;
static constexpr uint8_t REG_MDAC = 0x0C;
static constexpr uint8_t REG_DOSR_MSB = 0x0D;
static constexpr uint8_t REG_DOSR_LSB = 0x0E;
static constexpr uint8_t REG_CODEC_IF = 0x1B;
static constexpr uint8_t REG_PROCESSING_BLOCK = 0x3C;
static constexpr uint8_t REG_DAC_DATA_PATH = 0x3F;
static constexpr uint8_t REG_DAC_MUTE = 0x40;
static constexpr uint8_t REG_DAC_VOLUME_L = 0x41;
static constexpr uint8_t REG_DAC_VOLUME_R = 0x42;
// Page 1
static constexpr uint8_t REG_HP_DRIVERS = 0x1F;
static constexpr uint8_t REG_SPK_AMP = 0x20;
static constexpr uint8_t REG_HP_POP = 0x21;
static constexpr uint8_t REG_MIXER_ROUTING = 0x23;
static constexpr uint8_t REG_HPL_VOLUME = 0x24;
static constexpr uint8_t REG_HPR_VOLUME = 0x25;
static constexpr uint8_t REG_SPK_VOLUME = 0x26;
static constexpr uint8_t REG_HPL_DRIVER = 0x28;
static constexpr uint8_t REG_HPR_DRIVER = 0x29;
static constexpr uint8_t REG_SPK_DRIVER = 0x2A;

// HP driver power-on 1.22 s, 3.9 ms steps
static constexpr uint8_t HP_POP_SETTINGS = 0x4E;
static constexpr uint32_t OUTPUT_POWER_UP_MS = 1300;
// Output driver bits: 0 dB, D1 reserved as 1; D2 unmutes
static constexpr uint8_t HP_DRIVER_MUTED = 0x02;
static constexpr uint8_t HP_DRIVER_UNMUTED = 0x06;
static constexpr uint8_t SPK_DRIVER_UNMUTED = 0x04;
// Digital volume range in 0.5 dB steps: -63.5 dB to 0 dB
static constexpr int8_t VOLUME_MIN = -127;

void TLV320DAC3100::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    delay(1);
    this->reset_pin_->digital_write(true);
  }
  if (!this->write_page_(0, REG_RESET, 0x01)) {
    this->mark_failed(LOG_STR("Reset failed"));
    return;
  }
  // Internal memories initialize within 1 ms of a reset
  delay(1);

  const bool uses_pll = (this->clock_mux_ & 0x03) == 0x03;
  // P = 8 is written as 0
  const uint8_t pll_p_r = (uses_pll ? 0x80 : 0x00) | ((this->pll_p_ & 0x07) << 4) | (this->pll_r_ & 0x0F);
  const i2c::I2CRegValue clocks[] = {
      {REG_CLOCK_MUX, this->clock_mux_},
      {REG_PLL_J, this->pll_j_},
      // D MSB must be written immediately before D LSB
      {REG_PLL_D_MSB, static_cast<uint8_t>(this->pll_d_ >> 8)},
      {REG_PLL_D_LSB, static_cast<uint8_t>(this->pll_d_ & 0xFF)},
      {REG_PLL_P_R, pll_p_r},
      // Divider value 128 is written as 0
      {REG_NDAC, static_cast<uint8_t>(0x80 | (this->ndac_ & 0x7F))},
      {REG_MDAC, static_cast<uint8_t>(0x80 | (this->mdac_ & 0x7F))},
      {REG_DOSR_MSB, static_cast<uint8_t>((this->dosr_ >> 8) & 0x03)},
      {REG_DOSR_LSB, static_cast<uint8_t>(this->dosr_ & 0xFF)},
      {REG_CODEC_IF, static_cast<uint8_t>(this->word_length_ << 4)},
      {REG_PROCESSING_BLOCK, this->processing_block_},
  };
  // Drivers stay muted until the DAC is up; see power_up_dac_()
  const i2c::I2CRegValue outputs[] = {
      {REG_PAGE, 0x01},
      {REG_HP_POP, HP_POP_SETTINGS},
      // DAC_L and DAC_R to the mixer amplifiers
      {REG_MIXER_ROUTING, 0x44},
      {REG_HPL_DRIVER, HP_DRIVER_MUTED},
      {REG_HPR_DRIVER, HP_DRIVER_MUTED},
      {REG_SPK_DRIVER, static_cast<uint8_t>(this->speaker_gain_ << 3)},
      // Powered up, 1.35 V common mode
      {REG_HP_DRIVERS, static_cast<uint8_t>(this->has_headphone_() ? 0xC4 : 0x04)},
      {REG_SPK_AMP, static_cast<uint8_t>(this->has_speaker_() ? 0x86 : 0x06)},
      // Analog volume routed at 0 dB; level is set digitally
      {REG_HPL_VOLUME, 0x80},
      {REG_HPR_VOLUME, 0x80},
      {REG_SPK_VOLUME, 0x80},
      {REG_PAGE, 0x00},
  };
  if (!this->write_registers(clocks) || !this->write_registers(outputs)) {
    this->mark_failed(LOG_STR("Configuration failed"));
    return;
  }

  // DAC power-up needs the output ramp done and CODEC_CLKIN running (BCLK mode: I2S started)
  this->clock_running_ = !this->clocked_from_bclk_();
  this->set_timeout(OUTPUT_POWER_UP_MS, [this]() {
    this->outputs_ready_ = true;
    this->power_up_dac_();
  });
}

void TLV320DAC3100::on_audio_started() {
  this->clock_running_ = true;
  this->power_up_dac_();
}

bool TLV320DAC3100::clocked_from_bclk_() const {
  const uint8_t codec_clkin = this->clock_mux_ & 0x03;
  const uint8_t pll_clkin = (this->clock_mux_ >> 2) & 0x03;
  return codec_clkin == 0x01 || (codec_clkin == 0x03 && pll_clkin == 0x01);
}

void TLV320DAC3100::power_up_dac_() {
  if (this->dac_powered_ || !this->outputs_ready_ || !this->clock_running_ || this->is_failed())
    return;
  // Both channels on, soft-stepped volume; the mono speaker gets (L + R) / 2
  const uint8_t data_path = this->has_headphone_() ? 0xD4 : 0xF4;
  const uint8_t hp_driver = this->has_headphone_() ? HP_DRIVER_UNMUTED : HP_DRIVER_MUTED;
  const uint8_t spk_driver = (this->speaker_gain_ << 3) | (this->has_speaker_() ? SPK_DRIVER_UNMUTED : 0x00);
  if (!this->write_page_(0, REG_DAC_DATA_PATH, data_path) || !this->write_volume_() || !this->write_mute_() ||
      !this->write_page_(1, REG_HPL_DRIVER, hp_driver) || !this->write_byte(REG_HPR_DRIVER, hp_driver) ||
      !this->write_byte(REG_SPK_DRIVER, spk_driver) || !this->write_byte(REG_PAGE, 0x00)) {
    this->mark_failed(LOG_STR("DAC power-up failed"));
    return;
  }
  this->dac_powered_ = true;
}

void TLV320DAC3100::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TLV320DAC3100:\n"
                "  Clock mux: 0x%02X, PLL P=%u R=%u J=%u D=%u\n"
                "  NDAC=%u MDAC=%u DOSR=%u PRB_P%u",
                this->clock_mux_, this->pll_p_, this->pll_r_, this->pll_j_, this->pll_d_, this->ndac_, this->mdac_,
                this->dosr_, this->processing_block_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

bool TLV320DAC3100::set_mute_off() {
  this->is_muted_ = false;
  return this->write_mute_();
}

bool TLV320DAC3100::set_mute_on() {
  this->is_muted_ = true;
  return this->write_mute_();
}

bool TLV320DAC3100::set_volume(float volume) {
  this->volume_ = clamp(volume, 0.0f, 1.0f);
  return this->write_volume_();
}

bool TLV320DAC3100::write_page_(uint8_t page, uint8_t reg, uint8_t value) {
  return this->write_byte(REG_PAGE, page) && this->write_byte(reg, value);
}

bool TLV320DAC3100::write_mute_() {
  if (!this->write_page_(0, REG_DAC_MUTE, this->is_muted_ ? 0x0C : 0x00)) {
    ESP_LOGE(TAG, "Writing mute failed");
    return false;
  }
  return true;
}

bool TLV320DAC3100::write_volume_() {
  const auto volume = static_cast<int8_t>(lroundf(VOLUME_MIN * (1.0f - this->volume_)));
  if (!this->write_page_(0, REG_DAC_VOLUME_L, volume) || !this->write_byte(REG_DAC_VOLUME_R, volume)) {
    ESP_LOGE(TAG, "Writing volume failed");
    return false;
  }
  return true;
}

}  // namespace esphome::tlv320dac3100
