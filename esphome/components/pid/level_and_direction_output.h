#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/level_and_direction_output.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace pid {

/** Drives a level (0–1) output and a direction/reverse pin. Used by PID Climate
 * when level_and_direction_output is configured with level and direction. */
class LevelAndDirectionOutput : public Component, public output::LevelAndDirectionOutput {
 public:
  void set_level_output(output::FloatOutput *level) { this->level_ = level; }
  void set_direction_output(output::BinaryOutput *direction) { this->direction_ = direction; }

  void set_level(float level) override;
  void set_reverse(bool reverse) override;

  void dump_config() override;

 protected:
  output::FloatOutput *level_{nullptr};
  output::BinaryOutput *direction_{nullptr};
};

}  // namespace pid
}  // namespace esphome
