#include "custom_switch.h"

namespace esphome {
namespace diyless_opentherm {

void CustomSwitch::write_state(bool state)  {
  this->publish_state(state);
};

} // namespace diyless_opentherm
} // namespace esphome
