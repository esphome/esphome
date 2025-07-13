#pragma once

#include <map>

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "es8388_const.h"

namespace esphome {
namespace es8388 {

enum DacOutputLine : uint8_t {
  DAC_OUTPUT_LINE1,
  DAC_OUTPUT_LINE2,
  DAC_OUTPUT_BOTH,
};

enum AdcInputMicLine : uint8_t {
  ADC_INPUT_MIC_LINE1,
  ADC_INPUT_MIC_LINE2,
  ADC_INPUT_MIC_DIFFERENCE,
};

class ES8388 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
#ifdef USE_SELECT
  SUB_SELECT(dac_output)
  SUB_SELECT(adc_input_mic)
#endif

 public:
  /////////////////////////
  // Component overrides //
  /////////////////////////

  void setup() override;
  void dump_config() override;

  ////////////////////////
  // AudioDac overrides //
  ////////////////////////

  /// @brief Writes the volume out to the DAC
  /// @param volume floating point between 0.0 and 1.0
  /// @return True if successful and false otherwise
  bool set_volume(float volume) override;

  /// @brief Gets the current volume out from the DAC
  /// @return floating point between 0.0 and 1.0
  float volume() override;

  /// @brief Disables mute for audio out
  /// @return True if successful and false otherwise
  bool set_mute_off() override { return this->set_mute_state_(false); }

  /// @brief Enables mute for audio out
  /// @return True if successful and false otherwise
  bool set_mute_on() override { return this->set_mute_state_(true); }

  bool is_muted() override { return this->is_muted_; }

  optional<DacOutputLine> get_dac_power();
  optional<AdcInputMicLine> get_mic_input();

  bool set_dac_output(DacOutputLine line);
  bool set_adc_input_mic(AdcInputMicLine line);

 protected:
  /// @brief Mutes or unmutes the DAC audio out
  /// @param mute_state True to mute, false to unmute
  /// @return True if successful and false otherwise
  bool set_mute_state_(bool mute_state);
};

}  // namespace es8388
}  // namespace esphome
