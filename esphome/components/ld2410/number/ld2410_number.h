#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ld2410/ld2410.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410 {

class LD2410Number : public number::Number, public Component {
 public:
  LD2410Number(uint8_t num, enum LD2410NumType type) : gate_num_{num}, gate_type_{type} {};
  void setup() override;
  void dump_config() override;

  void set_ld2410_parent(LD2410Component *parent) { this->parent_ = parent; };

 protected:
  void control(float value) override;

  LD2410Component *parent_;
  uint8_t gate_num_{0};
  enum LD2410NumType gate_type_ { LD2410_THRES_MOVE };
};

}  // namespace ld2410
}  // namespace esphome
