#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/defines.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace at581x {

class AT581XComponent : public Component, public i2c::I2CDevice {
 public:
#ifdef USE_SWITCH
  void set_rf_power_switch(switch_::Switch *s) {
    this->rf_power_switch_ = s;
    s->turn_on();
  }
#endif

  void setup() override;
  void dump_config() override;
  //  float get_setup_priority() const override;

  void set_sensing_distance(int distance) { this->delta_ = 1023 - distance; }

  void set_rf_mode(bool enabled);
  void set_frequency(int frequency) { this->freq_ = frequency; }
  void set_poweron_selfcheck_time(int value) { this->self_check_time_ms_ = value; }
  void set_protect_time(int value) { this->protect_time_ms_ = value; }
  void set_trigger_base(int value) { this->trigger_base_time_ms_ = value; }
  void set_trigger_keep(int value) { this->trigger_keep_time_ms_ = value; }
  void set_stage_gain(int value) { this->gain_ = value; }
  void set_power_consumption(int value) { this->power_ = value; }

  bool i2c_write_config();
  bool reset_hardware_frontend();
  bool i2c_write_reg(uint8_t addr, uint8_t data);
  bool i2c_write_reg(uint8_t addr, uint32_t data);
  bool i2c_write_reg(uint8_t addr, uint16_t data);
  bool i2c_read_reg(uint8_t addr, uint8_t &data);

 protected:
#ifdef USE_SWITCH
  switch_::Switch *rf_power_switch_{nullptr};
#endif
  int freq_;
  int self_check_time_ms_;   /*!< Power-on self-test time, range: 0 ~ 65536 ms */
  int protect_time_ms_;      /*!< Protection time, recommended 1000 ms */
  int trigger_base_time_ms_; /*!< Default: 500 ms */
  int trigger_keep_time_ms_; /*!< Total trig time = TRIGGER_BASE_TIME + DEF_TRIGGER_KEEP_TIME, minimum: 1 */
  int delta_;                /*!< Delta value: 0 ~ 1023, the larger the value, the shorter the distance */
  int gain_;                 /*!< Default: 9dB */
  int power_;                /*!< In µA */
};

}  // namespace at581x
}  // namespace esphome
