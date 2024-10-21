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
#include "qn8027sub.h"

namespace esphome {
namespace qn8027 {

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

  QN8027_SUB_NUMBER(frequency, float)
  QN8027_SUB_NUMBER(deviation, float)
  QN8027_SUB_NUMBER(tx_pilot, uint8_t)
  QN8027_SUB_SWITCH(mute)
  QN8027_SUB_SWITCH(mono)
  QN8027_SUB_SWITCH(tx_enable)
  QN8027_SUB_SELECT(t1m_sel, T1mSel)
  QN8027_SUB_SWITCH(priv_en)
  QN8027_SUB_SELECT(pre_emphasis, PreEmphasis)
  QN8027_SUB_SELECT(input_impedance, InputImpedance)
  QN8027_SUB_NUMBER(input_gain, uint8_t)
  QN8027_SUB_NUMBER(digital_gain, uint8_t)
  QN8027_SUB_NUMBER(power_target, float)
  QN8027_SUB_SELECT(xtal_source, XtalSource)
  QN8027_SUB_NUMBER(xtal_current, float)
  QN8027_SUB_SELECT(xtal_frequency, XtalFrequency)
  QN8027_SUB_SWITCH(rds_enable)
  QN8027_SUB_NUMBER(rds_deviation, float)
  QN8027_SUB_TEXT(rds_station)
  QN8027_SUB_TEXT(rds_text)
  QN8027_SUB_SENSOR(aud_pk)
  QN8027_SUB_TEXT_SENSOR(fsm)
  QN8027_SUB_TEXT_SENSOR(chip_id)
  QN8027_SUB_SENSOR(reg30)
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...>, public Parented<QN8027Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->set_frequency(this->frequency_.value(x...)); }
};

}  // namespace qn8027
}  // namespace esphome
