#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "kt0803defs.h"
#include "kt0803sub.h"

namespace esphome {
namespace kt0803 {

class KT0803Component : public PollingComponent, public i2c::I2CDevice {
  ChipId chip_id_;  // no way to detect it
  bool reset_;
  union {
    struct KT0803State state_;
    uint8_t regs_[sizeof(struct KT0803State)];
  };

  bool check_reg_(uint8_t addr);
  void write_reg_(uint8_t addr);
  bool read_reg_(uint8_t addr);

  template<class S, class T> void publish(S *s, T state);
  // template specialization here is not supported by the compiler yet
  void publish_switch(switch_::Switch *s, bool state);
  void publish_select(select::Select *s, size_t index);

 public:
  KT0803Component();

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  KT0803_SUB_NUMBER(frequency, float)
  KT0803_SUB_SELECT(deviation, FrequencyDeviation)
  KT0803_SUB_SWITCH(mute)
  KT0803_SUB_SWITCH(mono)
  KT0803_SUB_SELECT(pre_emphasis, PreEmphasis)
  KT0803_SUB_NUMBER(pga, float)
  KT0803_SUB_NUMBER(rfgain, float)
  KT0803_SUB_SELECT(pilot_tone_amplitude, PilotToneAmplitude)
  KT0803_SUB_SELECT(bass_boost_control, BassBoostControl)
  KT0803_SUB_SWITCH(auto_pa_down)
  KT0803_SUB_SWITCH(pa_down)
  KT0803_SUB_SWITCH(standby_enable)
  KT0803_SUB_SWITCH(pa_bias)
  KT0803_SUB_SELECT(audio_limiter_level, AudioLimiterLevel)
  KT0803_SUB_SELECT(switch_mode, SwitchMode)
  KT0803_SUB_SWITCH(au_enhance)
  KT0803_SUB_SWITCH(ref_clk_enable)
  KT0803_SUB_SELECT(ref_clk_sel, ReferenceClock)
  KT0803_SUB_SWITCH(xtal_enable)
  KT0803_SUB_SELECT(xtal_sel, XtalSel)
  KT0803_SUB_SWITCH(alc_enable)
  KT0803_SUB_NUMBER(alc_gain, float)
  KT0803_SUB_SELECT(alc_attack_time, AlcTime)
  KT0803_SUB_SELECT(alc_decay_time, AlcTime)
  KT0803_SUB_SELECT(alc_hold_time, AlcHoldTime)
  KT0803_SUB_SELECT(alc_high, AlcHigh)
  KT0803_SUB_SELECT(alc_low, AlcLow)
  KT0803_SUB_SWITCH(silence_detection)
  KT0803_SUB_SELECT(silence_duration, SilenceLowAndHighLevelDurationTime)
  KT0803_SUB_SELECT(silence_high, SilenceHigh)
  KT0803_SUB_SELECT(silence_low, SilenceLow)
  KT0803_SUB_SELECT(silence_high_counter, SilenceHighLevelCounter)
  KT0803_SUB_SELECT(silence_low_counter, SilenceLowLevelCounter)
  KT0803_SUB_BINARY_SENSOR(pw_ok)
  KT0803_SUB_BINARY_SENSOR(slncid)

  void set_chip_id(ChipId value);
  ChipId get_chip_id();
  std::string get_chip_string() const;
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...>, public Parented<KT0803Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->set_frequency(this->frequency_.value(x...)); }
};

}  // namespace kt0803
}  // namespace esphome
