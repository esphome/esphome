#pragma once

#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/caravan_sensor_base.h"
#include "esphome/components/fendt_caravan/variable.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {

class FendtSwitch : public CaravanSensorBase<bool>, public switch_::Switch {
 public:
 protected:
  void write_state(bool state) override {
    if (this->variable_)
      this->variable_->set_value(state);
    this->parent_->on_switch_state_change_(this, state);
  }
  void on_decoded(const bool value) override { this->publish_state(value); }

 private:
};

}  // namespace esphome::fendt_caravan
#endif
