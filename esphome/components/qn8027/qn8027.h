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

class QN8027Component : public PollingComponent, public i2c::I2CDevice {
  std::string chip_id_;
  bool reset_;
  union {
    struct qn8027_state_t state_;
    uint8_t regs_[sizeof(struct qn8027_state_t)];
  };

  void write_reg_(uint8_t addr);
  bool read_reg_(uint8_t addr);

  std::string rds_station_;
  std::string rds_text_;
  uint8_t rds_station_pos_;
  uint8_t rds_text_pos_;
  uint8_t rds_upd_;

  void rds_update_();

  SUB_SENSOR(aud_pk)
  SUB_TEXT_SENSOR(fsm)
  SUB_TEXT_SENSOR(chip_id)
  SUB_NUMBER(frequency)
  SUB_NUMBER(frequency_deviation)
  SUB_NUMBER(tx_pilot)
  SUB_SWITCH(mute)
  SUB_SWITCH(mono)
  SUB_SWITCH(tx_enable)
  SUB_SELECT(t1m_sel)
  SUB_SWITCH(priv_en)
  SUB_SELECT(pre_emphasis)
  SUB_SELECT(xtal_source)
  SUB_NUMBER(xtal_current)
  SUB_SELECT(xtal_frequency)
  SUB_SELECT(input_impedance)
  SUB_NUMBER(input_gain)
  SUB_NUMBER(digital_gain)
  SUB_NUMBER(power_target)
  SUB_SWITCH(rds_enable)
  SUB_NUMBER(rds_frequency_deviation)
  SUB_TEXT(rds_station)
  SUB_TEXT(rds_text)

  void publish_aud_pk_();
  void publish_fsm_();
  void publish_chip_id_();
  void publish_frequency_();
  void publish_frequency_deviation_();
  void publish_mute_();
  void publish_mono_();
  void publish_tx_enable_();
  void publish_tx_pilot_();
  void publish_t1m_sel_();
  void publish_priv_en_();
  void publish_pre_emphasis_();
  void publish_xtal_source_();
  void publish_xtal_current_();
  void publish_xtal_frequency_();
  void publish_input_impedance_();
  void publish_input_gain_();
  void publish_digital_gain_();
  void publish_power_target_();
  void publish_rds_enable_();
  void publish_rds_frequency_deviation_();
  void publish_rds_station_();
  void publish_rds_text_();
  void publish_(number::Number *n, float state);
  void publish_(switch_::Switch *s, bool state);
  void publish_(select::Select *s, size_t index);
  void publish_(text::Text *t, const std::string &state);

 public:
  QN8027Component();

  // float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_frequency(float value);  // MHz
  float get_frequency();
  void set_frequency_deviation(float value);  // KHz
  float get_frequency_deviation();
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
  XtalSource get_xtal_source();
  void set_xtal_current(float value);
  float get_xtal_current();
  void set_xtal_frequency(XtalFrequency value);
  XtalFrequency get_xtal_frequency();
  void set_input_impedance(InputImpedance value);
  InputImpedance get_input_impedance();
  void set_input_gain(uint8_t value);
  uint8_t get_input_gain();
  void set_digital_gain(uint8_t value);
  uint8_t get_digital_gain();
  void set_power_target(float value);
  float get_power_target();
  void set_rds_enable(bool value);
  bool get_rds_enable();
  void set_rds_frequency_deviation(float value);
  float get_rds_frequency_deviation();
  void set_rds_station(const std::string &value);
  void set_rds_text(const std::string &value);
};

}  // namespace qn8027
}  // namespace esphome