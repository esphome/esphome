#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include <string>
#include "qn8027defs.h"

namespace esphome {
namespace qn8027 {

class QN8027Component : public PollingComponent, public i2c::I2CDevice {
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
  // TODO: text_sensor for CID*
  SUB_NUMBER(frequency)
  SUB_NUMBER(frequency_deviation)
  SUB_NUMBER(tx_pilot)
  SUB_NUMBER(t1m_sel)
  SUB_SWITCH(mute)
  SUB_SWITCH(mono)
  SUB_SWITCH(tx_enable)
  SUB_SELECT(tx_pilot)
  SUB_SELECT(t1m_sel)

  void publish_aud_pk_();
  void publish_fsm_();
  void publish_frequency_();
  void publish_frequency_deviation_();
  void publish_mute_();
  void publish_mono_();
  void publish_tx_enable_();
  void publish_tx_pilot_();
  void publish_t1m_sel_();

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
  void set_t1m_sel(uint8_t value); // 0 or 58-60
  T1mSel get_t1m_sel();
  void set_privacy_mode(bool b);
  void set_pre_emphasis(PreEmphasis us);
  void set_xtal_source(XtalSource source);
  void set_xtal_current(uint16_t uamps);
  void set_xtal_frequency(XtalFrequency mhz);
  void set_input_impedance(InputImpedance kohms);
  void set_input_gain(uint8_t dB);
  void set_digital_gain(uint8_t dB);
  void set_power_target(float dBuV);
  void set_rds_enable(bool b);
  void set_rds_frequency_deviation(float khz);
  void set_rds_station(const std::string &s);
  void set_rds_text(const std::string &s);
};

}  // namespace qn8027
}  // namespace esphome