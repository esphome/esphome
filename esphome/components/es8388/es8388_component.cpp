#include "es8388_component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <soc/io_mux_reg.h>

namespace esphome {
namespace es8388 {

static const char *const TAG = "es8388";

#define ES8388_CLK_MODE_SLAVE 0
#define ES8388_CLK_MODE_MASTER 1

void ES8388Component::setup() {
  switch (this->preset_) {
    case ES8388Preset::RASPIAUDIO_MUSE_LUXE:
      this->setup_raspiaudio_muse_luxe();
      break;

    case ES8388Preset::RASPIAUDIO_RADIO:
      this->setup_raspiaudio_radio();
      break;

    default:
      if (!this->init_instructions_.empty()) {
        for (std::array<uint8_t, 2> instruction : this->init_instructions_) {
          if (this->write_byte(instruction[0], instruction[1]))
            ESP_LOGW(TAG, "Error writing I2C instructions to ES8388: %#04x, %#04x", instruction[0], instruction[1]);
        }
      }
      break;
  }
}

void ES8388Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8388:");
  if (this->preset_) {
    ESP_LOGCONFIG(TAG, "  Preset loaded: %#04x", this->preset_);
  } else {
    if (!this->init_instructions_.empty()) {
      ESP_LOGCONFIG(TAG, "  %d initialization instructions found", this->init_instructions_.size());
    } else {
      ESP_LOGCONFIG(TAG, "  No initialization instructions found");
    }
  }
}

void ES8388Component::setup_raspiaudio_muse_luxe() {
  bool error = false;

  // reset
  error = error || not this->write_byte(0, 0x80);
  error = error || not this->write_byte(0, 0x00);
  // mute
  error = error || not this->write_byte(0x19, 0x04);
  // powerup
  error = error || not this->write_byte(0x01, 0x50);
  error = error || not this->write_byte(0x02, 0x00);
  // worker mode
  error = error || not this->write_byte(0x08, 0x00);
  // DAC powerdown
  error = error || not this->write_byte(0x04, 0xC0);
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

  // mono (L+R)/2
  error = error || not this->write_byte(29, 0x20);
  // ADC poweroff
  error = error || not this->write_byte(3, 0xFF);
  // ADC micboost 21 dB
  error = error || not this->write_byte(9, 0x77);

  // LINPUT1/RINPUT1
  error = error || not this->write_byte(10, 0x00);
  // i2S 16b
  error = error || not this->write_byte(12, 0x0C);
  // MCLK 256
  error = error || not this->write_byte(13, 0x02);
  // ADC high pass filter
  error = error || not this->write_byte(14, 0x30);
  // ADC attenuation 0 dB
  error = error || not this->write_byte(16, 0x00);
  error = error || not this->write_byte(17, 0x00);

  // noise gate -76.5dB
  error = error || not this->write_byte(22, 0x03);
  // ADC power
  error = error || not this->write_byte(3, 0x00);
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

  if (error) {
    ESP_LOGE(TAG, "Error writing I2C registers for preset Raspiaudio Muse Luxe");
  } else {
    ESP_LOGD(TAG, "I2C registers written successfully for preset Raspiaudio Muse Luxe");
  }
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
    ESP_LOGE(TAG, "Error writing I2C registers for preset Raspiaudio Radio");
  } else {
    ESP_LOGD(TAG, "I2C registers written successfully for preset Raspiaudio Radio");
  }
}

void ES8388Component::register_macro(std::string name, Instructions instructions) {
  Macro macro;
  macro.name = name;
  macro.instructions = instructions;
  this->macros_[name] = macro;
}

void ES8388Component::execute_macro(std::string name) {
  if (this->macros_.count(name) == 0) {
    ESP_LOGE(TAG, "Unable to execute macro `%s`: not found", name);
    return;
  }

  ESP_LOGD(TAG, "Calling ES8388 macro `%s` with %d I2C instructions", name.c_str(),
           this->macros_[name].instructions.size());

  for (std::array<uint8_t, 2> instruction : this->macros_[name].instructions) {
    if (this->write_byte(instruction[0], instruction[1]))
      ESP_LOGW(TAG, "Error writing I2C instructions to ES8388: %#04x, %#04x", instruction[0], instruction[1]);
  }
}

}  // namespace es8388
}  // namespace esphome
