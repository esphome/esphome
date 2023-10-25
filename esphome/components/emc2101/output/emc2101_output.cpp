#include "emc2101_output.h"

namespace esphome {
namespace emc2101 {

void EMC2101Output::write_state(float state) { this->parent_->set_duty_cycle(state); }

}  // namespace emc2101
}  // namespace esphome
