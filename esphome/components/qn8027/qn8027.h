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

 public:
  QN8027Component();

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  SUB_NUMBER_EX(frequency, float)
  SUB_NUMBER_EX(deviation, float)
  SUB_NUMBER_EX(tx_pilot, uint8_t)
  SUB_SWITCH_EX(mute)
  SUB_SWITCH_EX(mono)
  SUB_SWITCH_EX(tx_enable)
  SUB_SELECT_EX(t1m_sel, T1mSel)
  SUB_SWITCH_EX(priv_en)
  SUB_SELECT_EX(pre_emphasis, PreEmphasis)
  SUB_SELECT_EX(input_impedance, InputImpedance)
  SUB_NUMBER_EX(input_gain, uint8_t)
  SUB_NUMBER_EX(digital_gain, uint8_t)
  SUB_NUMBER_EX(power_target, float)
  SUB_SELECT_EX(xtal_source, XtalSource)
  SUB_NUMBER_EX(xtal_current, float)
  SUB_SELECT_EX(xtal_frequency, XtalFrequency)
  SUB_SWITCH_EX(rds_enable)
  SUB_NUMBER_EX(rds_deviation, float)
  SUB_TEXT_EX(rds_station)
  SUB_TEXT_EX(rds_text)
  SUB_SENSOR_EX(aud_pk)
  SUB_TEXT_SENSOR_EX(fsm)
  SUB_TEXT_SENSOR_EX(chip_id)
  SUB_SENSOR_EX(reg30)
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...>, public Parented<QN8027Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->set_frequency(this->frequency_.value(x...)); }
};

}  // namespace qn8027
}  // namespace esphome
