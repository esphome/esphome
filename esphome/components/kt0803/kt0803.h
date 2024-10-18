#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "kt0803defs.h"

namespace esphome {
namespace kt0803 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

#define SUB_NUMBER_EX(name, type) \
  SUB_NUMBER(name) \
  void publish_##name() { this->publish(this->name##_number_, (float) this->get_##name()); } \
  void set_##name(type value); \
  type get_##name();

#define SUB_SWITCH_EX(name) \
  SUB_SWITCH(name) \
  void publish_##name() { this->publish_switch(this->name##_switch_, this->get_##name()); } \
  void set_##name(bool value); \
  bool get_##name();

#define SUB_SELECT_EX(name, type) \
  SUB_SELECT(name) \
  void publish_##name() { this->publish_select(this->name##_select_, (size_t) this->get_##name()); } \
  void set_##name(type value); \
  type get_##name();

#define SUB_TEXT_EX(name) \
  SUB_TEXT(name) \
  void publish_##name() { this->publish(this->name##_text_, this->get_##name()); } \
  void set_##name(const std::string &value); \
  std::string get_##name();

#define SUB_SENSOR_EX(name) \
  SUB_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_sensor_, (float) this->get_##name()); } \
  float get_##name();

#define SUB_BINARY_SENSOR_EX(name) \
  SUB_BINARY_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_binary_sensor_, this->get_##name()); } \
  bool get_##name();

#define SUB_TEXT_SENSOR_EX(name) \
  SUB_TEXT_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_text_sensor_, this->get_##name()); } \
  std::string get_##name();

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

  SUB_NUMBER_EX(frequency, float)
  SUB_SELECT_EX(deviation, FrequencyDeviation)
  SUB_SWITCH_EX(mute)
  SUB_SWITCH_EX(mono)
  SUB_SELECT_EX(pre_emphasis, PreEmphasis)
  SUB_NUMBER_EX(pga, float)
  SUB_NUMBER_EX(rfgain, float)
  SUB_SELECT_EX(pilot_tone_amplitude, PilotToneAmplitude)
  SUB_SELECT_EX(bass_boost_control, BassBoostControl)
  SUB_SWITCH_EX(auto_pa_down)
  SUB_SWITCH_EX(pa_down)
  SUB_SWITCH_EX(standby_enable)
  SUB_SWITCH_EX(pa_bias)
  SUB_SELECT_EX(audio_limiter_level, AudioLimiterLevel)
  SUB_SELECT_EX(switch_mode, SwitchMode)
  SUB_SWITCH_EX(au_enhance)
  SUB_SWITCH_EX(ref_clk_enable)
  SUB_SELECT_EX(ref_clk, ReferenceClock)
  SUB_SWITCH_EX(xtal_enable)
  SUB_SELECT_EX(xtal_sel, XtalSel)
  SUB_SWITCH_EX(alc_enable)
  SUB_NUMBER_EX(alc_gain, float)
  SUB_SELECT_EX(alc_attack_time, AlcTime)
  SUB_SELECT_EX(alc_decay_time, AlcTime)
  SUB_SELECT_EX(alc_hold_time, AlcHoldTime)
  SUB_SELECT_EX(alc_high, AlcHigh)
  SUB_SELECT_EX(alc_low, AlcLow)
  SUB_SWITCH_EX(silence_detection)
  SUB_SELECT_EX(silence_duration, SilenceLowAndHighLevelDurationTime)
  SUB_SELECT_EX(silence_high, SilenceHigh)
  SUB_SELECT_EX(silence_low, SilenceLow)
  SUB_SELECT_EX(silence_high_counter, SilenceHighLevelCounter)
  SUB_SELECT_EX(silence_low_counter, SilenceLowLevelCounter)
  SUB_BINARY_SENSOR_EX(pw_ok)
  SUB_BINARY_SENSOR_EX(slncid)

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
