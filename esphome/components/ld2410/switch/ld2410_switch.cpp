#include "ld2410_switch.h"

namespace esphome {
namespace ld2410 {

LD2410Switch::LD2410Switch() {}

void LD2410Switch::write_state(bool state) { this->publish_state(state); }

}  // namespace ld2410
}  // namespace esphome
