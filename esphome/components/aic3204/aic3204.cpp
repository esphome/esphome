#include "aic3204.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::aic3204 {

ESPHOME_LOG_TAG(TAG, "aic3204");

#define ERROR_CHECK(err, msg) \
  if (!(err)) { \
    this->mark_failed(LOG_STR(msg)); \
    return; \
  }

static constexpr i2c::I2CRegValue AIC3204_INIT[] PROGMEM = {
    // Set register page to 0
    {AIC3204_PAGE_CTRL, 0x00},
    // Initiate SW reset (PLL is powered off as part of reset)
    {AIC3204_SW_RST, 0x01},
    // *** Program clock settings ***
    // Default is CODEC_CLKIN is from MCLK pin. Don't need to change this.
    // MDAC*NDAC*FOSR*48Khz = mClk (24.576 MHz when the XMOS is expecting 48kHz audio)
    // (See page 51 of https://www.ti.com/lit/ml/slaa557/slaa557.pdf)
    // We do need MDAC*DOSR/32 >= the resource compute level for the processing block
    // So here 2*128/32 = 8, which is equal to processing block 1 's resource compute
    // See page 5 of https://www.ti.com/lit/an/slaa404c/slaa404c.pdf for the workflow
    // for determining these settings.
    // Power up NDAC and set to 2
    {AIC3204_NDAC, 0x82},
    // Power up MDAC and set to 2
    {AIC3204_MDAC, 0x82},
    // Program DOSR = 128
    {AIC3204_DOSR, 0x80},
    // Set Audio Interface Config: I2S, 32 bits, DOUT always driving
    {AIC3204_CODEC_IF, 0x30},
    // For I2S Firmware only, set SCLK/MFP3 pin as Audio Data In
    {AIC3204_SCLK_MFP3, 0x02},
    {AIC3204_AUDIO_IF_4, 0x01},
    {AIC3204_AUDIO_IF_5, 0x01},
    // Program the DAC processing block to be used - PRB_P1
    {AIC3204_DAC_SIG_PROC, 0x01},
    // *** Select Page 1 ***
    {AIC3204_PAGE_CTRL, 0x01},
    // Enable the internal AVDD_LDO:
    {AIC3204_LDO_CTRL, 0x09},
    // *** Program Analog Blocks ***
    // Disable Internal Crude AVdd in presence of external AVdd supply or before powering up internal AVdd LDO
    {AIC3204_PWR_CFG, 0x08},
    // Enable Master Analog Power Control
    {AIC3204_LDO_CTRL, 0x01},
    // Page 125: Common mode control register, set d6 to 1 to make the full chip common mode = 0.75 v
    // We are using the internal AVdd regulator with a nominal output of 1.72 V (see LDO_CTRL_REGISTER on page 123)
    // Page 86 says to only set the common mode voltage to 0.9 v if AVdd >= 1.8... but it isn't on our hardware
    // We also adjust the HPL and HPR gains to -2dB gian later in this config flow compensate (see page 47)
    // (All pages refer to the TLV320AIC3204 Application Reference Guide)
    {AIC3204_CM_CTRL, 0x40},
    // *** Set PowerTune Modes ***
    // Set the Left & Right DAC PowerTune mode to PTM_P3/4. Use Class-AB driver.
    {AIC3204_PLAY_CFG1, 0x00},
    {AIC3204_PLAY_CFG2, 0x00},
    // Set the REF charging time to 40ms
    {AIC3204_REF_STARTUP, 0x01},
    // HP soft stepping settings for optimal pop performance at power up
    // Rpop used is 6k with N = 6 and soft step = 20usec. This should work with 47uF coupling
    // capacitor. Can try N=5,6 or 7 time constants as well. Trade-off delay vs “pop” sound.
    {AIC3204_HP_START, 0x25},
    // Route Left DAC to HPL
    {AIC3204_HPL_ROUTE, 0x08},
    // Route Right DAC to HPR
    {AIC3204_HPR_ROUTE, 0x08},
    // Route Left DAC to LOL
    {AIC3204_LOL_ROUTE, 0x08},
    // Route Right DAC to LOR
    {AIC3204_LOR_ROUTE, 0x08},
    // Unmute HPL and set gain to -2dB (see comment before configuring the AIC3204_CM_CTRL register)
    {AIC3204_HPL_GAIN, 0x3e},
    // Unmute HPR and set gain to -2dB (see comment before configuring the AIC3204_CM_CTRL register)
    {AIC3204_HPR_GAIN, 0x3e},
    // Unmute LOL and set gain to 0dB
    {AIC3204_LOL_DRV_GAIN, 0x00},
    // Unmute LOR and set gain to 0dB
    {AIC3204_LOR_DRV_GAIN, 0x00},
    // Power up HPL and HPR, LOL and LOR drivers
    {AIC3204_OP_PWR_CTRL, 0x3C},
};

void AIC3204::setup() {
  ERROR_CHECK(this->write_registers(AIC3204_INIT), "Failed to initialize");

  // Wait for 2.5 sec for soft stepping to take effect before attempting power-up
  this->set_timeout(2500, [this]() {
    // *** Power Up DAC ***
    // Select Page 0
    ERROR_CHECK(this->write_byte(AIC3204_PAGE_CTRL, 0x00), "Set PAGE_CTRL failed");
    // Power up the Left and Right DAC Channels. Route Left data to Left DAC and Right data to Right DAC.
    // DAC Vol control soft step 1 step per DAC word clock.
    ERROR_CHECK(this->write_byte(AIC3204_DAC_CH_SET1, 0xd4), "Set DAC_CH_SET1 failed");
    // Set left and right DAC digital volume control
    ERROR_CHECK(this->write_volume_(), "Set volume failed");
    // Unmute left and right channels
    ERROR_CHECK(this->write_mute_(), "Set mute failed");
  });
}

void AIC3204::dump_config() {
  ESP_LOGCONFIG(TAG, "AIC3204:");
  LOG_I2C_DEVICE(this);
}

bool AIC3204::set_mute_off() {
  this->is_muted_ = false;
  return this->write_mute_();
}

bool AIC3204::set_mute_on() {
  this->is_muted_ = true;
  return this->write_mute_();
}

bool AIC3204::set_auto_mute_mode(uint8_t auto_mute_mode) {
  this->auto_mute_mode_ = auto_mute_mode & 0x07;
  ESP_LOGVV(TAG, "Setting auto_mute_mode to 0x%.2x", this->auto_mute_mode_);
  return this->write_mute_();
}

bool AIC3204::set_volume(float volume) {
  this->volume_ = clamp<float>(volume, 0.0, 1.0);
  return this->write_volume_();
}

bool AIC3204::is_muted() { return this->is_muted_; }

float AIC3204::volume() { return this->volume_; }

bool AIC3204::write_mute_() {
  uint8_t mute_mode_byte = this->auto_mute_mode_ << 4;  // auto-mute control is bits 4-6
  mute_mode_byte |= this->is_muted_ ? 0x0c : 0x00;      // mute bits are 2-3
  if (!this->write_byte(AIC3204_PAGE_CTRL, 0x00) || !this->write_byte(AIC3204_DAC_CH_SET2, mute_mode_byte)) {
    ESP_LOGE(TAG, "Writing mute modes failed");
    return false;
  }
  return true;
}

bool AIC3204::write_volume_() {
  const int8_t dvc_min_byte = -127;
  const int8_t dvc_max_byte = 48;

  int8_t volume_byte = dvc_min_byte + (this->volume_ * (dvc_max_byte - dvc_min_byte));
  volume_byte = clamp<int8_t>(volume_byte, dvc_min_byte, dvc_max_byte);

  ESP_LOGVV(TAG, "Setting volume to 0x%.2x", volume_byte & 0xFF);

  if ((!this->write_byte(AIC3204_PAGE_CTRL, 0x00)) || (!this->write_byte(AIC3204_DACL_VOL_D, volume_byte)) ||
      (!this->write_byte(AIC3204_DACR_VOL_D, volume_byte))) {
    ESP_LOGE(TAG, "Writing volume failed");
    return false;
  }
  return true;
}

}  // namespace esphome::aic3204
