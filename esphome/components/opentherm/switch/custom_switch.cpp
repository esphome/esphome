#include "custom_switch.h"

namespace esphome {
namespace opentherm {

void CustomSwitch::write_state(bool state) { this->publish_state(state); };

}  // namespace opentherm
}  // namespace esphome
