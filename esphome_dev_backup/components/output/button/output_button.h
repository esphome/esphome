#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace output {

class OutputButton : public button::Button, public Component {
 public:
  void dump_config() override;

  void set_output(BinaryOutput *output) { output_ = output; }
  void set_duration(uint32_t duration) { duration_ = duration; }

 protected:
  void press_action() override;

  output::BinaryOutput *output_;
  uint32_t duration_;
};

}  // namespace output
}  // namespace esphome
