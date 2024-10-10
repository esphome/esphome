#include "reset_radar_button.h"

namespace esphome {
namespace seeed_mr60fda2 {

void ResetRadarButton::press_action() { this->parent_->reset_radar(); }

}  // namespace seeed_mr60fda2
}  // namespace esphome