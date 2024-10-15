#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "si4713defs.h"

namespace esphome {
namespace si4713 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

class Si4713Component : public PollingComponent, public i2c::I2CDevice {
  std::string chip_id_;
  InternalGPIOPin *reset_pin_;
  bool reset_;
  bool gpio_[3];
  /*
    union {
      struct Si4713State state_;
      uint8_t regs_[sizeof(struct Si4713State)];
    };

    void write_reg_(uint8_t addr);
    bool read_reg_(uint8_t addr);
  */
  bool send_cmd(const void *cmd, size_t cmd_size, void *res, size_t res_size);

  template<typename CMD> bool send_cmd(const CMD &cmd) {
    return this->send_cmd((const void *) &cmd, sizeof(cmd), nullptr, 0);
  }

  template<typename CMD, typename RES> bool send_cmd(CMD cmd, RES &res) {
    return this->send_cmd((const void *) &cmd, sizeof(cmd), (void *) &res, sizeof(res));
  }

  template<typename P> bool set_prop(P p) {
    CmdSetProperty cmd = CmdSetProperty(p.PROP, p.PROPD);
    return this->send_cmd(cmd);
  }

  template<typename P> bool get_prop(P &p) {
    ResGetProperty res;
    if (this->send_cmd(CmdGetProperty(p.PROP), res)) {
      p.PROPD = ((uint16_t) res.PROPDH << 8) | res.PROPDL;
      return true;
    }

    return false;
  }

  std::string rds_station_;
  std::string rds_text_;
  uint8_t rds_station_pos_;
  uint8_t rds_text_pos_;
  uint8_t rds_upd_;

  void rds_update_();

  SUB_TEXT_SENSOR(chip_id)
  // TODO: sensors TX_TUNE_STATUS / FREQ, RFuV, ANTCAP, NL
  // TODO: sensors TX_ASQ_STATUS / OVERMOD, IALH, IALL, INLEVEL
  SUB_NUMBER(frequency)
  SUB_SWITCH(mute)
  SUB_SWITCH(mono)
  SUB_SWITCH(rds_enable)
  SUB_TEXT(rds_station)
  SUB_TEXT(rds_text)
  SUB_SWITCH(gpio1)
  SUB_SWITCH(gpio2)
  SUB_SWITCH(gpio3)

  void publish_();
  void publish_chip_id();
  void publish_frequency();
  void publish_mute();
  void publish_mono();
  void publish_rds_enable();
  void publish_rds_station();
  void publish_rds_text();
  void publish_gpio(uint8_t pin);
  void publish(sensor::Sensor *s, float state);
  void publish(text_sensor::TextSensor *s, const std::string &state);
  void publish(number::Number *n, float state);
  void publish(switch_::Switch *s, bool state);
  void publish(select::Select *s, size_t index);
  void publish(text::Text *t, const std::string &state);

 public:
  Si4713Component();

  // float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  bool reset();
  bool power_up();
  bool power_down();
  bool detect_chip_id();
  bool tune_freq(uint16_t freq);
  bool tune_power(uint8_t power = 0, uint8_t antcap = 0);
  bool tune_wait();

  void set_reset_pin(InternalGPIOPin *pin);
  void set_frequency(float value);
  float get_frequency();
  void set_mute(bool value);
  bool get_mute();
  void set_mono(bool value);
  bool get_mono();
  void set_rds_enable(bool value);
  bool get_rds_enable();
  void set_rds_station(const std::string &value);
  void set_rds_text(const std::string &value);
  void set_gpio(uint8_t pin, bool value);
  bool get_gpio(uint8_t pin);
};

}  // namespace si4713
}  // namespace esphome
