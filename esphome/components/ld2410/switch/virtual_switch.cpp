#include "virtual_switch.h"

namespace esphome {
namespace ld2410 {

VirtualSwitch::VirtualSwitch() {}

void VirtualSwitch::write_state(bool state) { this->publish_state(state); }

}  // namespace ld2410
}  // namespace esphome
