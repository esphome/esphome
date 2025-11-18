/**
 * @file axp2101_number.h
 * @brief Number component for AXP2101 voltage control
 */
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "axp2101.h"

namespace esphome {
namespace axp2101 {

/**
 * @brief Number component for controlling AXP2101 power rail voltages
 *
 * This number component allows setting the output voltage of individual
 * power rails (DCDCs and LDOs).
 */
class AXP2101Number : public number::Number, public Component {
 public:
  void set_parent(AXP2101Component *parent) { this->parent_ = parent; }
  void set_power_rail(PowerRail rail) { this->rail_ = rail; }
  void set_voltage_range(uint16_t min_mv, uint16_t max_mv, uint16_t step_mv) {
    this->min_mv_ = min_mv;
    this->max_mv_ = max_mv;
    this->step_mv_ = step_mv;
  }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  AXP2101Component *parent_;
  PowerRail rail_;
  uint16_t min_mv_{500};
  uint16_t max_mv_{3500};
  uint16_t step_mv_{100};
};

}  // namespace axp2101
}  // namespace esphome
