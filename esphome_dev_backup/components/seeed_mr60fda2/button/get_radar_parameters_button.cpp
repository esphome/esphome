#include "get_radar_parameters_button.h"

namespace esphome {
namespace seeed_mr60fda2 {

void GetRadarParametersButton::press_action() { this->parent_->get_radar_parameters(); }

}  // namespace seeed_mr60fda2
}  // namespace esphome
