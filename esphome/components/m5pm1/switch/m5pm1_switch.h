#pragma once

#include "esphome/components/switch/switch.h"
#include "../m5pm1.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::m5pm1 {

class AW8737ASwitch : public switch_::Switch, public Component, public Parented<M5PM1Component> {
 public:
  void setup() override;
  void dump_config() override;
  void set_pin(M5PM1GPIOPin *pin) { this->pin_ = pin; }
  void set_pulse_num(uint8_t pulse_num) { this->pulse_num_ = pulse_num; }

 protected:
  void write_state(bool state) override;
  M5PM1GPIOPin *pin_{nullptr};
  uint8_t pulse_num_{3};
};

enum class M5PM1PowerSwitchType : uint8_t {
  M5PM1_POWER_SWITCH_TYPE_CHARGER,
  M5PM1_POWER_SWITCH_TYPE_DCDC_3V3,
  M5PM1_POWER_SWITCH_TYPE_LDO_3V3,
  M5PM1_POWER_SWITCH_TYPE_BOOST_5V,
  M5PM1_POWER_SWITCH_TYPE_HOLD_LDO_3V3,
  M5PM1_POWER_SWITCH_TYPE_HOLD_BOOST_5V,
  M5PM1_POWER_SWITCH_TYPE_HOLD_GPIO,
};

class M5PM1PowerSwitch : public switch_::Switch, public Component {
 public:
  M5PM1PowerSwitch(M5PM1Component *parent, M5PM1PowerSwitchType type) : parent_(parent), type_(type) {}
  M5PM1PowerSwitch(M5PM1Component *parent, M5PM1PowerSwitchType type, uint8_t pin)
      : parent_(parent), type_(type), pin_(pin) {}

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  bool is_hold_switch_() const;
  bool is_gpio_hold_switch_() const;
  uint8_t get_mask_() const;

  M5PM1Component *parent_;
  M5PM1PowerSwitchType type_;
  uint8_t pin_{0};
};

}  // namespace esphome::m5pm1
