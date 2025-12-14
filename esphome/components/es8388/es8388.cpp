#include "es8388.h"

#include <cinttypes>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace es8388 {

static const char *const TAG = "es8388";

// Mark the component as failed; use only in setup
#define ES8388_ERROR_FAILED(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }

// Return false; use outside of setup
#define ES8388_ERROR_CHECK(func) \
  if (!(func)) { \
    return false; \
  }

void ES8388::setup() {
  // mute DAC
  this->set_mute_state_(true);

  // I2S worker mode
  ES8388_ERROR_FAILED(this->write_byte(ES8388_MASTERMODE, 0x00));

  /* Chip Control and Power Management */
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CONTROL2, 0x50));
  // normal all and power up all
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CHIPPOWER, 0x00));

  // vmidsel/500k
  // EnRef=0,Play&Record Mode,(0x17-both of mic&play)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CONTROL1, 0x12));

  // i2s 16 bits
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL1, 0x18));
  // sample freq 256
  // DACFsMode,SINGLE SPEED; DACFsRatio,256
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL2, 0x02));
  // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL16, 0x00));
  // only left DAC to left mixer enable 0db
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL17, 0x90));
  // only right DAC to right mixer enable 0db
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL20, 0x90));
  // set internal ADC and DAC use the same LRCK clock, ADC LRCK as internal LRCK
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL21, 0x80));
  // vroi=0 - 1.5k VREF to analog output resistance (default)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL23, 0x00));

  // power down adc and line in
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCPOWER, 0xFF));

  //@nightdav
  ES8388_ERROR_FAILED(
      this->write_byte(ES8388_ADCCONTROL1, 0x00));  // +21dB : recommended value for ALC & voice recording

  // set to Mono Right
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL3, 0x02));

  // I2S 16 Bits length and I2S serial audio data format
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL4, 0x0d));
  // ADCFsMode,singel SPEED,RATIO=256
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL5, 0x02));

  // ADC Volume
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL8, 0x00));
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL9, 0x00));

  //@nightDav
  // ALC Config (as recommended by ES8388 user guide for voice recording)

  // Reg 0x12 = 0xe2 (ALC enable, PGA Max. Gain=23.5dB, Min. Gain=0dB)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL10, 0xe2));

  // Reg 0x13 = 0xa0 (ALC Target=-1.5dB, ALC Hold time =0 mS)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL11, 0xa0));
  // Reg 0x14 = 0x12(Decay time =820uS , Attack time = 416 uS)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL12, 0x12));

  // Reg 0x15 = 0x06(ALC mode)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL13, 0x06));

  // Reg 0x16 = 0xc3(nose gate = -40.5dB, NGG = 0x01(mute ADC))
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL14, 0xc3));

  // Power on ADC
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL21, 0x80));

  // Start state machine
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CHIPPOWER, 0xF0));
  delay(1);
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CHIPPOWER, 0x00));

  // DAC volume max
  // Set initial volume
  // this->set_volume(0.75);  // 0.75 = 0xBF = 0dB

  this->set_mute_state_(false);

  // unmute ADC with fade in
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL7, 0x60));
  // unmute DAC with fade in
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL3, 0x20));

  // Power on ADC, Enable LIN&RIN, Power off MICBIAS, set int1lp to low power mode
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCPOWER, 0x09));

#ifdef USE_SELECT
  if (this->dac_output_select_ != nullptr) {
    auto dac_power = this->get_dac_power();
    if (dac_power.has_value()) {
      auto dac_power_str = this->dac_output_select_->at(dac_power.value());
      if (dac_power_str.has_value()) {
        this->dac_output_select_->publish_state(dac_power_str.value());
      } else {
        ESP_LOGW(TAG, "Unknown DAC output power value: %d", dac_power.value());
      }
    }
  }
  if (this->adc_input_mic_select_ != nullptr) {
    auto mic_input = this->get_mic_input();
    if (mic_input.has_value()) {
      auto mic_input_str = this->adc_input_mic_select_->at(mic_input.value());
      if (mic_input_str.has_value()) {
        this->adc_input_mic_select_->publish_state(mic_input_str.value());
      } else {
        ESP_LOGW(TAG, "Unknown ADC input mic value: %d", mic_input.value());
      }
    }
  }
#endif
}

void ES8388::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8388 Audio Codec:");
  LOG_I2C_DEVICE(this);
#ifdef USE_SELECT
  LOG_SELECT("  ", "DacOutputSelect", this->dac_output_select_);
  LOG_SELECT("  ", "ADCInputMicSelect", this->adc_input_mic_select_);
#endif

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize");
    return;
  }
}

bool ES8388::set_volume(float volume) {
  volume = clamp(volume, 0.0f, 1.0f);
  uint8_t value = remap<uint8_t, float>(volume, 0.0f, 1.0f, -96, 0);
  ESP_LOGD(TAG, "Setting ES8388_DACCONTROL4 / ES8388_DACCONTROL5 to 0x%02X (volume: %f)", value, volume);
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL4, value));
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL5, value));

  return true;
}

float ES8388::volume() {
  uint8_t value;
  ES8388_ERROR_CHECK(this->read_byte(ES8388_DACCONTROL4, &value));
  return remap<float, uint8_t>(value, -96, 0, 0.0f, 1.0f);
}

bool ES8388::set_mute_state_(bool mute_state) {
  uint8_t value = 0;

  this->is_muted_ = mute_state;

  ES8388_ERROR_CHECK(this->read_byte(ES8388_DACCONTROL3, &value));
  ESP_LOGV(TAG, "Read ES8388_DACCONTROL3: 0x%02X", value);

  if (mute_state) {
    value = 0x3C;
  }

  ESP_LOGV(TAG, "Setting ES8388_DACCONTROL3 to 0x%02X (muted: %s)", value, YESNO(mute_state));
  return this->write_byte(ES8388_DACCONTROL3, value);
}

// Set dac power output
bool ES8388::set_dac_output(DacOutputLine line) {
  uint8_t reg_out1 = 0;
  uint8_t reg_out2 = 0;
  uint8_t dac_power = 0;

  // 0x00: -30dB , 0x1E: 0dB
  switch (line) {
    case DAC_OUTPUT_LINE1:
      reg_out1 = 0x1E;
      dac_power = ES8388_DAC_OUTPUT_LOUT1_ROUT1;
      break;
    case DAC_OUTPUT_LINE2:
      reg_out2 = 0x1E;
      dac_power = ES8388_DAC_OUTPUT_LOUT2_ROUT2;
      break;
    case DAC_OUTPUT_BOTH:
      reg_out1 = 0x1E;
      reg_out2 = 0x1E;
      dac_power = ES8388_DAC_OUTPUT_BOTH;
      break;
    default:
      ESP_LOGE(TAG, "Unknown DAC output line: %d", line);
      return false;
  };

  ESP_LOGV(TAG, "Setting ES8388_DACPOWER to 0x%02X", dac_power);
  ESP_LOGV(TAG, "Setting ES8388_DACCONTROL24 / ES8388_DACCONTROL25 to 0x%02X", reg_out1);
  ESP_LOGV(TAG, "Setting ES8388_DACCONTROL26 / ES8388_DACCONTROL27  to 0x%02X", reg_out2);

  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL24, reg_out1));  // LOUT1VOL
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL25, reg_out1));  // ROUT1VOL
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL26, reg_out2));  // LOUT2VOL
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL27, reg_out2));  // ROUT1VOL

  return this->write_byte(ES8388_DACPOWER, dac_power);
}

optional<DacOutputLine> ES8388::get_dac_power() {
  uint8_t dac_power;
  if (!this->read_byte(ES8388_DACPOWER, &dac_power)) {
    this->status_momentary_warning("dacpower_read");
    return {};
  }
  switch (dac_power) {
    case ES8388_DAC_OUTPUT_LOUT1_ROUT1:
      return DAC_OUTPUT_LINE1;
    case ES8388_DAC_OUTPUT_LOUT2_ROUT2:
      return DAC_OUTPUT_LINE2;
    case ES8388_DAC_OUTPUT_BOTH:
      return DAC_OUTPUT_BOTH;
    default:
      return {};
  }
}

// Set ADC input MIC
bool ES8388::set_adc_input_mic(AdcInputMicLine line) {
  uint8_t mic_input = 0;

  switch (line) {
    case ADC_INPUT_MIC_LINE1:
      mic_input = ES8388_ADC_INPUT_LINPUT1_RINPUT1;
      break;
    case ADC_INPUT_MIC_LINE2:
      mic_input = ES8388_ADC_INPUT_LINPUT2_RINPUT2;
      break;
    case ADC_INPUT_MIC_DIFFERENCE:
      mic_input = ES8388_ADC_INPUT_DIFFERENCE;
      break;
    default:
      ESP_LOGE(TAG, "Unknown ADC input mic line: %d", line);
      return false;
  }

  ESP_LOGV(TAG, "Setting ES8388_ADCCONTROL2 to 0x%02X", mic_input);
  ES8388_ERROR_CHECK(this->write_byte(ES8388_ADCCONTROL2, mic_input));

  return true;
}

optional<AdcInputMicLine> ES8388::get_mic_input() {
  uint8_t mic_input;
  if (!this->read_byte(ES8388_ADCCONTROL2, &mic_input)) {
    this->status_momentary_warning("adccontrol2_read");
    return {};
  }
  switch (mic_input) {
    case ES8388_ADC_INPUT_LINPUT1_RINPUT1:
      return ADC_INPUT_MIC_LINE1;
    case ES8388_ADC_INPUT_LINPUT2_RINPUT2:
      return ADC_INPUT_MIC_LINE2;
    case ES8388_ADC_INPUT_DIFFERENCE:
      return ADC_INPUT_MIC_DIFFERENCE;
    default:
      return {};
  };
}

}  // namespace es8388
}  // namespace esphome
