#include "get_radar_parameters_button.h"

namespace esphome::seeed_mr60fda2 {

void GetRadarParametersButton::press_action() { this->parent_->get_radar_parameters(); }

}  // namespace esphome::seeed_mr60fda2
