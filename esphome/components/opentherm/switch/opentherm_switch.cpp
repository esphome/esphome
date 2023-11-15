#include "opentherm_switch.h"

namespace esphome {
namespace opentherm {

void OpenThermSwitch::write_state(bool state) { this->publish_state(state); };

}  // namespace opentherm
}  // namespace esphome
