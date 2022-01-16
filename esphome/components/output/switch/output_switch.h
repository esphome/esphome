#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace output {

enum OutputSwitchRestoreMode {
  OUTPUT_SWITCH_RESTORE_DEFAULT_OFF,
  OUTPUT_SWITCH_RESTORE_DEFAULT_ON,
  OUTPUT_SWITCH_ALWAYS_OFF,
  OUTPUT_SWITCH_ALWAYS_ON,
  OUTPUT_SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
  OUTPUT_SWITCH_RESTORE_INVERTED_DEFAULT_ON,
};

class OutputSwitch : public switch_::Switch, public Component {
 public:
  void set_output(BinaryOutput *output) { output_ = output; }

  void set_restore_mode(OutputSwitchRestoreMode restore_mode) { restore_mode_ = restore_mode; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  output::BinaryOutput *output_;
  OutputSwitchRestoreMode restore_mode_;
};

}  // namespace output
}  // namespace esphome
