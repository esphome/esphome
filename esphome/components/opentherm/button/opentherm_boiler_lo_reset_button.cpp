#include "opentherm_boiler_lo_reset_button.h"

namespace esphome {
namespace opentherm {

void OpenThermBoilerLOResetButton::press_action() { this->parent_->boiler_lo_reset(); }

}  // namespace opentherm
}  // namespace esphome
