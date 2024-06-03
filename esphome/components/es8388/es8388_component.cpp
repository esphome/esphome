#include "es8388_component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <soc/io_mux_reg.h>

namespace esphome {
namespace es8388 {

#define ES8388_CLK_MODE_SLAVE 0
#define ES8388_CLK_MODE_MASTER 1

void ES8388Component::setup() {
  int zerooo = 0;
  int val = 2 / zerooo;
  ESP_LOGW("ES8388", "Writing I2C registers");

  // PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
  // PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, 1);
  // WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL) & 0xFFFFFFF0);

  this->setup_raspiaudio_radio();
}

void ES8388Component::setup_raspiaudio_radio() {
  bool error = false;
  error = error || not this->write_byte(0, 0x80);
  error = error || not this->write_byte(0, 0x00);
  // mute
  error = error || not this->write_byte(25, 0x04);
  error = error || not this->write_byte(1, 0x50);
  // powerup
  error = error || not this->write_byte(2, 0x00);
  // slave mode
  error = error || not this->write_byte(8, 0x00);
  // DAC powerdown
  error = error || not this->write_byte(4, 0xC0);
  // vmidsel/500k ADC/DAC idem
  error = error || not this->write_byte(0, 0x12);

  error = error || not this->write_byte(1, 0x00);
  // i2s 16 bits
  error = error || not this->write_byte(23, 0x18);
  // sample freq 256
  error = error || not this->write_byte(24, 0x02);
  // LIN2/RIN2 for mixer
  error = error || not this->write_byte(38, 0x09);
  // left DAC to left mixer
  error = error || not this->write_byte(39, 0x80);
  // right DAC to right mixer
  error = error || not this->write_byte(42, 0x80);
  // DACLRC ADCLRC idem
  error = error || not this->write_byte(43, 0x80);
  error = error || not this->write_byte(45, 0x00);
  // DAC volume max
  error = error || not this->write_byte(27, 0x00);
  error = error || not this->write_byte(26, 0x00);

  // mono (L+R)/2
  error = error || not this->write_byte(29, 0x00);

  // DAC power-up LOUT1/ROUT1 ET 2 enabled
  error = error || not this->write_byte(4, 0x39);

  // DAC R phase inversion
  error = error || not this->write_byte(28, 0x14);

  // ADC poweroff
  error = error || not this->write_byte(3, 0xFF);

  // ADC amp 24dB
  error = error || not this->write_byte(9, 0x88);

  // differential input
  error = error || not this->write_byte(10, 0xFC);
  error = error || not this->write_byte(11, 0x02);

  // Select LIN2and RIN2 as differential input pairs
  // error = error || not this->write_byte(11,0x82);

  // i2S 16b
  error = error || not this->write_byte(12, 0x0C);
  // MCLK 256
  error = error || not this->write_byte(13, 0x02);
  // ADC high pass filter
  // error = error || not this->write_byte(14,0x30);

  // ADC Volume LADC volume = 0dB
  error = error || not this->write_byte(16, 0x00);

  // ADC Volume RADC volume = 0dB
  error = error || not this->write_byte(17, 0x00);

  // ALC
  error =
      error || not this->write_byte(0x12, 0xfd);  // Reg 0x12 = 0xe2 (ALC enable, PGA Max. Gain=23.5dB, Min. Gain=0dB)
  // error = error || not this->write_byte(0x12, 0x22); // Reg 0x12 = 0xe2 (ALC enable, PGA Max. Gain=23.5dB, Min.
  // Gain=0dB)
  error = error || not this->write_byte(0x13, 0xF9);  // Reg 0x13 = 0xc0 (ALC Target=-4.5dB, ALC Hold time =0 mS)
  error = error || not this->write_byte(0x14, 0x02);  // Reg 0x14 = 0x12(Decay time =820uS , Attack time = 416 uS)
  error = error || not this->write_byte(0x15, 0x06);  // Reg 0x15 = 0x06(ALC mode)
  error = error || not this->write_byte(0x16, 0xc3);  // Reg 0x16 = 0xc3(nose gate = -40.5dB, NGG = 0x01(mute ADC))
  error = error ||
          not this->write_byte(0x02, 0x55);  // Reg 0x16 = 0x55 (Start up DLL, STM and Digital block for recording);

  // error = error || not this->write_byte(3, 0x09);
  error = error || not this->write_byte(3, 0x00);

  // reset power DAC and ADC
  error = error || not this->write_byte(2, 0xF0);
  error = error || not this->write_byte(2, 0x00);

  // unmute
  error = error || not this->write_byte(25, 0x00);
  // amp validation
  error = error || not this->write_byte(46, 30);
  error = error || not this->write_byte(47, 30);
  error = error || not this->write_byte(48, 33);
  error = error || not this->write_byte(49, 33);

  if (error) {
    ESP_LOGE("ES8388", "Error writing I2C registers!");
  } else {
    ESP_LOGW("ES8388", "I2C registers written successfully");
  }
}

void ES8388Component::setup_raspiaudio_muse_luxe() {
  // mute
  this->mute();

  // powerup
  this->powerup();
  this->clock_mode(ES8388_CLK_MODE_SLAVE);

  bool error = false;

  // DAC powerdown
  this->powerdown_dac();
  // vmidsel/500k ADC/DAC idem
  error = error || not this->write_byte(0x00, 0x12);

  // i2s 16 bits
  error = error || not this->write_byte(0x17, 0x18);
  // sample freq 256
  error = error || not this->write_byte(0x18, 0x02);
  // LIN2/RIN2 for mixer
  error = error || not this->write_byte(0x26, 0x00);
  // left DAC to left mixer
  error = error || not this->write_byte(0x27, 0x90);
  // right DAC to right mixer
  error = error || not this->write_byte(0x2A, 0x90);
  // DACLRC ADCLRC idem
  error = error || not this->write_byte(0x2B, 0x80);
  error = error || not this->write_byte(0x2D, 0x00);
  // DAC volume max
  error = error || not this->write_byte(0x1B, 0x00);
  error = error || not this->write_byte(0x1A, 0x00);

  // ADC poweroff
  error = error || not this->write_byte(0x03, 0xFF);
  // ADC amp 24dB
  error = error || not this->write_byte(0x09, 0x88);
  // LINPUT1/RINPUT1
  error = error || not this->write_byte(0x0A, 0x00);
  // ADC mono left
  error = error || not this->write_byte(0x0B, 0x02);
  // i2S 16b
  error = error || not this->write_byte(0x0C, 0x0C);
  // MCLK 256
  error = error || not this->write_byte(0x0D, 0x02);
  // ADC Volume
  error = error || not this->write_byte(0x10, 0x00);
  error = error || not this->write_byte(0x11, 0x00);
  // ALC OFF
  error = error || not this->write_byte(0x03, 0x09);
  error = error || not this->write_byte(0x2B, 0x80);

  error = error || not this->write_byte(0x02, 0xF0);
  delay(1);
  error = error || not this->write_byte(0x02, 0x00);
  // DAC power-up LOUT1/ROUT1 enabled
  error = error || not this->write_byte(0x04, 0x30);
  error = error || not this->write_byte(0x03, 0x00);
  // DAC volume max
  error = error || not this->write_byte(0x2E, 0x1C);
  error = error || not this->write_byte(0x2F, 0x1C);
  // unmute
  error = error || not this->write_byte(0x19, 0x00);
}

void ES8388Component::powerup_dac() { this->write_byte(0x04, 0x3B); }
// void ES8388Component::powerup_adc() {}
void ES8388Component::powerup() {
  this->write_byte(0x01, 0x50);  // LPVrefBuf - low power
  this->write_byte(0x02, 0x00);  // power up DAC/ADC without resetting DMS, DEM, filters & serial
}

void ES8388Component::powerdown_dac() { this->write_byte(0x04, 0xC0); }
void ES8388Component::powerdown_adc() {}
void ES8388Component::powerdown() {
  this->powerdown_dac();
  this->powerdown_adc();
}

void ES8388Component::clock_mode(uint8_t mode) {
  if (mode == ES8388_CLK_MODE_SLAVE) {
    this->write_byte(0x08, 0x00);
  } else {
    this->write_byte(0x08, 0x80);
    // TODO multipliers
  }
}

void ES8388Component::mute() { this->write_byte(0x19, 0x04); }

}  // namespace es8388
}  // namespace esphome
