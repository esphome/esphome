#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "../tm1638.h"

namespace esphome {
namespace tm1638 {

class TM1638OutputLed : public output::BinaryOutput, public Component {
 public:
  void dump_config() override;

  void set_tm1638(TM1638Component *tm1638) { tm1638_ = tm1638; }
  void set_lednum(int led) { led_ = led; }

 protected:
  void write_state(bool state) override;

  TM1638Component *tm1638_;
  int led_;
};

}  // namespace tm1638
}  // namespace esphome
