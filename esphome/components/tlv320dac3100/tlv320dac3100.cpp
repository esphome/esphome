#include "tlv320dac3100.h"

#include <cmath>

#include "esphome/core/gpio_pin.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tlv320dac3100 {

ESPHOME_LOG_TAG(TAG, "tlv320dac3100");

// Page 0
static constexpr uint8_t REG_PAGE = 0x00;
static constexpr uint8_t REG_RESET = 0x01;
static constexpr uint8_t REG_DAC_DATA_PATH = 0x3F;
static constexpr uint8_t REG_DAC_MUTE = 0x40;
static constexpr uint8_t REG_DAC_VOLUME_L = 0x41;
static constexpr uint8_t REG_DAC_VOLUME_R = 0x42;
// Page 1
static constexpr uint8_t REG_HPL_DRIVER = 0x28;
static constexpr uint8_t REG_HPR_DRIVER = 0x29;
static constexpr uint8_t REG_SPK_DRIVER = 0x2A;

// Covers the 1.22 s HP driver power-on set up in the setup table
static constexpr uint32_t OUTPUT_POWER_UP_MS = 1300;
// Digital volume range in 0.5 dB steps: -63.5 dB to 0 dB
static constexpr int8_t VOLUME_MIN = -127;

void TLV320DAC3100::setup() {
  pulse_reset(this->reset_pin_, {.low_ms = 1});
  if (!this->write_byte(REG_PAGE, 0x00) || !this->write_byte(REG_RESET, 0x01)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  // Internal memories initialize within 1 ms of a reset; delay(1) can end sooner
  delay(2);
  if (!this->write_registers(this->setup_table_, this->setup_count_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // DAC power-up needs the output ramp done and CODEC_CLKIN running; see on_audio_started()
  this->set_timeout(OUTPUT_POWER_UP_MS, [this]() {
    this->outputs_ready_ = true;
    this->power_up_dac_();
  });
}

void TLV320DAC3100::on_audio_started() {
  this->clock_running_ = true;
  this->power_up_dac_();
}

void TLV320DAC3100::power_up_dac_() {
  if (this->dac_powered_ || !this->outputs_ready_ || !this->clock_running_ || this->is_failed())
    return;
  const i2c::I2CRegValue data_path[] = {{REG_PAGE, 0x00}, {REG_DAC_DATA_PATH, this->data_path_}};
  const i2c::I2CRegValue drivers[] = {{REG_PAGE, 0x01},
                                      {REG_HPL_DRIVER, this->hp_driver_},
                                      {REG_HPR_DRIVER, this->hp_driver_},
                                      {REG_SPK_DRIVER, this->spk_driver_},
                                      {REG_PAGE, 0x00}};
  // Levels before the output drivers; on failure dac_powered_ stays false, so the next audio start retries
  this->dac_powered_ = this->status_comm_result(
      this->write_registers(data_path) && this->write_levels_() && this->write_registers(drivers),
      LOG_STR("DAC power-up failed"));
}

void TLV320DAC3100::dump_config() {
  ESP_LOGCONFIG(TAG, "TLV320DAC3100:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

bool TLV320DAC3100::set_mute_off() {
  this->is_muted_ = false;
  return this->write_levels_();
}

bool TLV320DAC3100::set_mute_on() {
  this->is_muted_ = true;
  return this->write_levels_();
}

bool TLV320DAC3100::set_volume(float volume) {
  this->volume_ = clamp(volume, 0.0f, 1.0f);
  return this->write_levels_();
}

bool TLV320DAC3100::write_levels_() {
  if (this->is_failed())
    return false;
  const auto volume = static_cast<uint8_t>(lroundf(VOLUME_MIN * (1.0f - this->volume_)));
  const i2c::I2CRegValue levels[] = {
      {REG_PAGE, 0x00},
      {REG_DAC_MUTE, static_cast<uint8_t>(this->is_muted_ ? 0x0C : 0x00)},
      {REG_DAC_VOLUME_L, volume},
      {REG_DAC_VOLUME_R, volume},
  };
  const bool ok = this->write_registers(levels);
  // Until the DAC is up, power_up_dac_() owns the warning status
  return this->dac_powered_ ? this->status_comm_result(ok) : ok;
}

}  // namespace esphome::tlv320dac3100
