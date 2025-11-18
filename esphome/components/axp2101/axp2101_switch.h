/**
 * @file axp2101_switch.h
 * @brief Switch component for AXP2101 power rail control
 */
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "axp2101.h"

namespace esphome {
namespace axp2101 {

/**
 * @brief Switch component for controlling AXP2101 power rails
 *
 * This switch allows enabling/disabling individual power rails (DCDCs and LDOs).
 */
class AXP2101Switch : public switch_::Switch, public Component {
 public:
  void set_parent(AXP2101Component *parent) { this->parent_ = parent; }
  void set_power_rail(PowerRail rail) { this->rail_ = rail; }

  void dump_config() override;

 protected:
  void write_state(bool state) override;

  AXP2101Component *parent_;
  PowerRail rail_;
};

}  // namespace axp2101
}  // namespace esphome
