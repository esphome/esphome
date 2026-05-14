#include "reset_radar_button.h"

namespace esphome::seeed_mr60fda2 {

void ResetRadarButton::press_action() { this->parent_->factory_reset(); }

}  // namespace esphome::seeed_mr60fda2
