#include "ch_water_filling_button.h"

namespace esphome {
namespace opentherm {

void CHWaterFillingButton::press_action() { this->parent_->ch_water_filling(); }

}  // namespace opentherm
}  // namespace esphome
