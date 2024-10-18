#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "qn8027defs.h"

namespace esphome {
namespace qn8027 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

#define SUB_NUMBER_EX(name) \
  SUB_NUMBER(name) \
  void publish_##name() { this->publish(this->name##_number_, (float) this->get_##name()); }

#define SUB_SWITCH_EX(name) \
  SUB_SWITCH(name) \
  void publish_##name() { this->publish_switch(this->name##_switch_, this->get_##name()); }

#define SUB_SELECT_EX(name) \
  SUB_SELECT(name) \
  void publish_##name() { this->publish_select(this->name##_select_, (size_t) this->get_##name()); }

#define SUB_TEXT_EX(name) \
  SUB_TEXT(name) \
  void publish_##name() { this->publish(this->name##_text_, this->get_##name()); }

#define SUB_SENSOR_EX(name) \
  SUB_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_sensor_, (float) this->get_##name()); }

#define SUB_BINARY_SENSOR_EX(name) \
  SUB_BINARY_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_binary_sensor_, this->get_##name()); }

#define SUB_TEXT_SENSOR_EX(name) \
  SUB_TEXT_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_text_sensor_, this->get_##name()); }

class QN8027Component : public PollingComponent, public i2c::I2CDevice {
  std::string chip_id_;
  bool reset_;
  union {
    struct QN8027State state_;
    uint8_t regs_[sizeof(struct QN8027State)];
  };
  uint8_t reg30_;  // undocumented diagnostic register
  std::string rds_station_;
  std::string rds_text_;
  uint8_t rds_station_pos_;
  uint8_t rds_text_pos_;
  uint8_t rds_upd_;

  void write_reg_(uint8_t addr);
  bool read_reg_(uint8_t addr);
  void rds_update_();

  template<class S, class T> void publish(S *s, T state);
  // template specialization here is not supported by the compiler yet
  void publish_switch(switch_::Switch *s, bool state);
  void publish_select(select::Select *s, size_t index);

  SUB_NUMBER_EX(frequency)
  SUB_NUMBER_EX(deviation)
  SUB_NUMBER_EX(tx_pilot)
  SUB_SWITCH_EX(mute)
  SUB_SWITCH_EX(mono)
  SUB_SWITCH_EX(tx_enable)
  SUB_SELECT_EX(t1m_sel)
  SUB_SWITCH_EX(priv_en)
  SUB_SELECT_EX(pre_emphasis)
  SUB_SELECT_EX(input_impedance)
  SUB_NUMBER_EX(input_gain)
  SUB_NUMBER_EX(digital_gain)
  SUB_NUMBER_EX(power_target)
  SUB_SELECT_EX(xtal_source)
  SUB_NUMBER_EX(xtal_current)
  SUB_SELECT_EX(xtal_frequency)
  SUB_SWITCH_EX(rds_enable)
  SUB_NUMBER_EX(rds_deviation)
  SUB_TEXT_EX(rds_station)
  SUB_TEXT_EX(rds_text)
  SUB_SENSOR_EX(aud_pk)
  SUB_TEXT_SENSOR_EX(fsm)
  SUB_TEXT_SENSOR_EX(chip_id)
  SUB_SENSOR_EX(reg30)

 public:
  QN8027Component();

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  // config
  void set_frequency(float value);  // MHz
  float get_frequency();
  void set_deviation(float value);  // kHz
  float get_deviation();
  void set_mute(bool value);
  bool get_mute();
  void set_mono(bool value);
  bool get_mono();
  void set_tx_enable(bool value);
  bool get_tx_enable();
  void set_tx_pilot(uint8_t value);
  uint8_t get_tx_pilot();
  void set_t1m_sel(T1mSel value);
  T1mSel get_t1m_sel();
  void set_priv_en(bool value);
  bool get_priv_en();
  void set_pre_emphasis(PreEmphasis value);
  PreEmphasis get_pre_emphasis();
  void set_xtal_source(XtalSource value);
  void set_input_impedance(InputImpedance value);
  InputImpedance get_input_impedance();
  void set_input_gain(uint8_t value);
  uint8_t get_input_gain();
  void set_digital_gain(uint8_t value);
  uint8_t get_digital_gain();
  void set_power_target(float value);
  float get_power_target();
  XtalSource get_xtal_source();
  void set_xtal_current(float value);
  float get_xtal_current();
  void set_xtal_frequency(XtalFrequency value);
  XtalFrequency get_xtal_frequency();
  void set_rds_enable(bool value);
  bool get_rds_enable();
  void set_rds_deviation(float value);
  float get_rds_deviation();
  void set_rds_station(const std::string &value);
  std::string get_rds_station();
  void set_rds_text(const std::string &value);
  std::string get_rds_text();

  // used by sensors
  float get_aud_pk();
  std::string get_fsm();
  std::string get_chip_id();
  uint8_t get_reg30();
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...>, public Parented<QN8027Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->set_frequency(this->frequency_.value(x...)); }
};

}  // namespace qn8027
}  // namespace esphome
