#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace tuya {

class TuyaNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_number_id(uint8_t number_id) { this->number_id_ = number_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;

  Tuya *parent_;
  uint8_t number_id_{0};
};

}  // namespace tuya
}  // namespace esphome
